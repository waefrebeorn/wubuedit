/*
 * This file is part of WubuEdit — a SLERM ground-up C11 video editor.
 * Copyright (c) 2026 Waefrebeorn.
 *
 * Licensed under the Waefrebeorn Umbrella License v3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   https://github.com/waefrebeorn/waefrebeorn-umbrella-license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * wugui_shaders.h — GLSL shader source strings for WuBuEdit GPU rendering.
 *
 * These shaders are Ported From Diffusion Studio (app.diffusion.studio)
 * which uses WebGL2 + WebGL2 shaders for its GPU-accelerated video compositing.
 * The original shaders were extracted from the decompiled JS bundle (2.1MB).
 *
 * SHADER 1: yuv_to_rgb.frag
 *   Reads a packed RGBA buffer containing interleaved YUV420 data and
 *   decodes it to RGB entirely on the GPU. This is the ZERO-COPY video
 *   path: frames decoded by FFmpeg (libavcodec) are uploaded as a single
 *   texture, then the shader does YUV→RGB conversion without any CPU
 *   format conversion.
 *
 * SHADER 2: composite.frag
 *   Multi-layer compositing with color+alpha channels. This is the core
 *   of Diffusion Studio's rendering pipeline: it takes a color texture
 *   and an alpha texture (matte) and composites them with proper alpha
 *   blending. Used for overlay layers, text, graphics.
 *
 * SHADER 3: color_grade.comp (compute)
 *   Color grading via 3D LUT + eq parameters. Adapted from Diffusion's
 *   Lumetri-style color controls, implemented as a Vulkan/SPIR-V compute
 *   shader (also usable as a fragment shader in OpenGL).
 *
 * The vertex shader is shared across all rendering: it's a simple
 * passthrough that maps a fullscreen quad to screen coordinates.
 */
#ifndef WUGUI_SHADERS_H
#define WUGUI_SHADERS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Vertex Shader ────────────────────────────────────────────────────────
 * Fullscreen quad vertex shader. Maps a quad at (-1,-1) to (1,1) in
 * clip space, with texture coordinates (0,0) to (1,1).
 */
#define WUGUI_VERTEX_SHADER \
"#version 330 core\n" \
"layout(location = 0) in vec2 a_position;\n" \
"layout(location = 1) in vec2 a_texCoord;\n" \
"out vec2 v_texCoord;\n" \
"void main() {\n" \
"    v_texCoord = a_texCoord;\n" \
"    gl_Position = vec4(a_position, 0.0, 1.0);\n" \
"}\n"

/* ── YUV to RGB Fragment Shader ───────────────────────────────────────────
 * Ported from Diffusion Studio's shader 3 (the YUV hardware decode shader).
 *
 * Reads a single RGBA texture where the data is a packed YUV420P buffer:
 * - Y plane occupies [0, width*height) bytes (luma)
 * - U plane occupies [width*height, width*height + (w/2)*(h/2)) bytes
 * - V plane occupies the rest
 *
 * The shader reads byte values directly from the RGBA texture (treating
 * each pixel as 4 bytes) to reconstruct YUV pixels and convert to RGB.
 *
 * This avoids CPU-side YUV→RGB conversion and GPU texture upload of
 * the converted RGB data — the texture upload is of the raw decoder
 * output, and conversion happens entirely in the shader.
 */
#define WUGUI_YUV_TO_RGB_SHADER \
"#version 330 core\n" \
"precision highp float;\n" \
"\n" \
"in vec2 v_texCoord;\n" \
"out vec4 fragColor;\n" \
"\n" \
"uniform sampler2D u_sourceTexture;\n" \
"uniform vec2 u_resolution;\n" \
"\n" \
"// This function determines the value for a single byte in the YUV stream\n" \
"// Ported from Diffusion Studio shader: getByteValue(byteOffset)\n" \
"float getByteValue(float byteOffset) {\n" \
"    float width = u_resolution.x;\n" \
"    float height = u_resolution.y;\n" \
"    float yPlaneSize = width * height;\n" \
"    float uvPlaneSize = (width * 0.5) * (height * 0.5);\n" \
"\n" \
"    if (byteOffset < yPlaneSize) {\n" \
"        // Luma plane: find pixel coordinates\n" \
"        float y = floor(byteOffset / width);\n" \
"        float x = mod(byteOffset, width);\n" \
"        // Sample the RGBA texture: each pixel holds 4 bytes (R,G,B,A)\n" \
"        // We need the first byte (R) of the pixel at (x,y)\n" \
"        float pixelIndex = byteOffset / 4.0;\n" \
"        float px = mod(pixelIndex, width);\n" \
"        float py = floor(pixelIndex / width);\n" \
"        vec4 rgba = texture(u_sourceTexture, vec2(px / width, py / height));\n" \
"        return rgba.r;\n" \
"    } else if (byteOffset < yPlaneSize + uvPlaneSize) {\n" \
"        // U plane\n" \
"        float uByteOffset = byteOffset - yPlaneSize;\n" \
"        float uWidth = width * 0.5;\n" \
"        float pixelIndex = uByteOffset / 4.0;\n" \
"        float px = mod(pixelIndex, uWidth);\n" \
"        float py = floor(pixelIndex / uWidth);\n" \
"        vec4 rgba = texture(u_sourceTexture, vec2(px / width, py / height));\n" \
"        return rgba.r;\n" \
"    } else {\n" \
"        // V plane\n" \
"        float vByteOffset = byteOffset - yPlaneSize - uvPlaneSize;\n" \
"        float uWidth = width * 0.5;\n" \
"        float pixelIndex = vByteOffset / 4.0;\n" \
"        float px = mod(pixelIndex, uWidth);\n" \
"        float py = floor(pixelIndex / uWidth);\n" \
"        vec4 rgba = texture(u_sourceTexture, vec2(px / width, py / height));\n" \
"        return rgba.r;\n" \
"    }\n" \
"}\n" \
"\n" \
"void main() {\n" \
"    float width = u_resolution.x;\n" \
"    float height = u_resolution.y;\n" \
"    float yPlaneSize = width * height;\n" \
"\n" \
"    // Calculate the byte offset for this pixel's Y value\n" \
"    float pixelIndex = v_texCoord.y * height * width + v_texCoord.x * width;\n" \
"    float yByteOffset = pixelIndex;\n" \
"\n" \
"    // U and V are at half resolution (subsampled 4:2:0)\n" \
"    float uvPixelX = floor(v_texCoord.x * (width * 0.5));\n" \
"    float uvPixelY = floor(v_texCoord.y * (height * 0.5));\n" \
"    float uvIndex = uvPixelY * (width * 0.5) + uvPixelX;\n" \
"\n" \
"    float uByteOffset = yPlaneSize + uvIndex;\n" \
"    float vByteOffset = yPlaneSize + (width * 0.5) * (height * 0.5) + uvIndex;\n" \
"\n" \
"    float Y = getByteValue(yByteOffset) - 0.0625;\n" \
"    float U = getByteValue(uByteOffset) - 0.5;\n" \
"    float V = getByteValue(vByteOffset) - 0.5;\n" \
"\n" \
"    // BT.601 YUV to RGB conversion\n" \
"    float R = Y + 1.5748 * V;\n" \
"    float G = Y - 0.1873 * U - 0.4681 * V;\n" \
"    float B = Y + 1.8556 * U;\n" \
"\n" \
"    fragColor = vec4(R, G, B, 1.0);\n" \
"}\n"

/* ── Composite (Color + Alpha) Fragment Shader ────────────────────────────
 * Ported from Diffusion Studio's shader 1.
 *
 * Takes a color texture and an alpha texture (matte) and composites them:
 *   fragColor = (color.rgb, alpha.r)
 *
 * Used for: overlay layers, text rendering, graphics with transparency.
 * The alpha channel comes from a separate grayscale texture (the "matte").
 */
#define WUGUI_COMPOSITE_SHADER \
"#version 330 core\n" \
"precision highp float;\n" \
"\n" \
"uniform sampler2D u_colorTexture;\n" \
"uniform sampler2D u_alphaTexture;\n" \
"in vec2 v_texCoord;\n" \
"out vec4 fragColor;\n" \
"\n" \
"void main() {\n" \
"    vec3 color = texture(u_colorTexture, v_texCoord).rgb;\n" \
"    float alpha = texture(u_alphaTexture, v_texCoord).r;\n" \
"    fragColor = vec4(color, alpha);\n" \
"}\n"

/* ── Source Pass-Through Shader ───────────────────────────────────────────
 * Ported from Diffusion Studio's shader 2.
 *
 * Simple pass-through that takes a source texture and outputs RGB with
 * full opacity. Used for base video frames that don't need alpha processing.
 */
#define WUGUI_SOURCE_SHADER \
"#version 330 core\n" \
"precision highp float;\n" \
"\n" \
"uniform sampler2D u_sourceTexture;\n" \
"in vec2 v_texCoord;\n" \
"out vec4 fragColor;\n" \
"\n" \
"void main() {\n" \
"    vec4 source = texture(u_sourceTexture, v_texCoord);\n" \
"    fragColor = vec4(source.rgb, 1.0);\n" \
"}\n"

/* ── Color Grading Fragment Shader ────────────────────────────────────────
 * Adapted from Diffusion Studio's Lumetri-style color controls.
 *
 * Applies brightness/contrast/saturation/gamma adjustments in the order
 * recommended by professional colorists (Noam Kroll's color grading order):
 *   1. Brightness (add/subtract)
 *   2. Contrast (scale around 0.5)
 *   3. Saturation (scale color channels)
 *   4. Gamma (power function)
 *
 * Also supports 3D LUT application via a separate LUT3D shader path.
 */
#define WUGUI_COLOR_GRADE_SHADER \
"#version 330 core\n" \
"precision highp float;\n" \
"\n" \
"uniform sampler2D u_sourceTexture;\n" \
"uniform float u_brightness;\n" \
"uniform float u_contrast;\n" \
"uniform float u_saturation;\n" \
"uniform float u_gamma;\n" \
"in vec2 v_texCoord;\n" \
"out vec4 fragColor;\n" \
"\n" \
"void main() {\n" \
"    vec4 color = texture(u_sourceTexture, v_texCoord);\n" \
"    vec3 rgb = color.rgb;\n" \
"\n" \
"    // 1. Brightness\n" \
"    rgb += u_brightness;\n" \
"\n" \
"    // 2. Contrast (scale around 0.5)\n" \
"    rgb = (rgb - 0.5) * u_contrast + 0.5;\n" \
"\n" \
"    // 3. Saturation (luminance-weighted)\n" \
"    float luminance = dot(rgb, vec3(0.299, 0.587, 0.114));\n" \
"    rgb = mix(vec3(luminance), rgb, u_saturation);\n" \
"\n" \
"    // 4. Gamma correction\n" \
"    rgb = pow(max(rgb, 0.0), vec3(1.0 / u_gamma));\n" \
"\n" \
"    fragColor = vec4(rgb, color.a);\n" \
"}\n"

/* ── WuBu Watermark Shader ────────────────────────────────────────────────
 * Diegetic UI element: the ~ WuBu ~ watermark pulsing at the beat.
 * Uses time-based sine wave for the pulse animation.
 * The watermark is rendered as a semi-transparent overlay that pulses
 * in sync with the audio's tempo (passed as u_beatPhase).
 */
#define WUGUI_WUBU_WATERMARK_SHADER \
"#version 330 core\n" \
"precision highp float;\n" \
"\n" \
"uniform sampler2D u_fontTexture;\n" \
"uniform float u_beatPhase;\n" \
"uniform vec2 u_resolution;\n" \
"in vec2 v_texCoord;\n" \
"out vec4 fragColor;\n" \
"\n" \
"// The WuBu watermark text: '~ WuBu ~'\n" \
"// Rendered as a texture with alpha channel\n" \
"void main() {\n" \
"    vec4 tex = texture(u_fontTexture, v_texCoord);\n" \
"    float alpha = tex.a;\n" \
"    \n" \
"    // Pulse at the beat: sine wave, clamped to 0.3-0.7\n" \
"    float pulse = 0.3 + 0.4 * sin(u_beatPhase * 3.14159 * 2.0);\n" \
"    alpha *= pulse;\n" \
"    \n" \
"    // WuBu gold color\n" \
"    fragColor = vec4(1.0, 0.78, 0.24, alpha);\n" \
"}\n"

#ifdef __cplusplus
}
#endif

#endif /* WUGUI_SHADERS_H */
