---
license: creativeml.openrail++
license: cc-by-4.0
---
# Waefrebeorn WuBuWizard — Cleveland Video Dataset (Parody)

## Dataset Description

This dataset contains all video media, audio, and supporting files for the
**WuBuWizard** film project — a parody/comedy multimedia production by Waefrebeorn.

**Copyright notice:** All original Waefrebeorn creative work is licensed under
the Waefrebeorn Umbrella License v3.0. Transformative parody elements derived
from third-party source material are licensed under CC BY 4.0 (attribution)
in combination with the Waefrebeorn Umbrella License v3.0, to the extent
permitted by parody/fair use doctrine.

## License Combination

This dataset uses a **dual/combination license** model:

1. **Waefrebeorn Umbrella License v3.0** (primary) — covers all original Waefrebeorn
   creative work. See `LICENSE` for full text.
   - SPDX: `Waefrebeorn-Umbrella-3.0`
   - URL: https://github.com/waefrebeorn/waefrebeorn-umbrella-license

2. **Creative Commons Attribution 4.0 International** (secondary) — covers
   transformative parody elements that incorporate or derive from third-party
   copyrighted source material, to the extent permitted by fair use/parody doctrine.
   - SPDX: `CC-BY-4.0`
   - URL: https://creativecommons.org/licenses/by/4.0/

3. **Parody/Fair Use** — this dataset contains parody works that transform
   original copyrighted material for comedic/social commentary purposes.
   All such use is claimed as fair use under 17 U.S.C. § 107 and equivalent
   provisions in other jurisdictions. Source attribution is provided where
   known.

## Dataset Structure

```
wubuwizard-cleveland-video/
├── LICENSE                  # Waefrebeorn Umbrella License v3.0 (full text)
├── LICENSE_PARODY           # CC BY 4.0 for parody/transformative elements
├── README.md                # This file (Hugging Face dataset card)
├── dataset_infos.json       # Hugging Face dataset metadata
├── tracks/                  # Video tracks (organized by song/track)
│   ├── track01/             # Track 01 media
│   │   └── clips/           # Individual clip files
│   ├── track02/
│   └── ...
├── audio/                   # Sound design, music stems
├── lyrics/                  # Lyric files and caption data
└── metadata/                # Per-clip metadata, shot classifications
```

## Usage

This dataset is intended for:
- Video editing research (WubuEdit training)
- Parody/comedy content analysis
- Creative reuse in transformative works
- Educational purposes

## Citation

If you use this dataset, please cite:
```
@dataset{waefrebeorn_cleveland_video,
  author       = {waefrebeorn},
  title        = {WuBuWizard Cleveland Video Dataset (Parody)},
  year         = {2026},
  publisher    = {Hugging Face},
  license      = {CC-BY-4.0 and Waefrebeorn-Umbrella-3.0 and parody/fair-use},
}
```

## Contact

For licensing questions, commercial use, or compliance:
waefrebeorn@waefrebeorn.org

---
language:
- en
license: cc-by-4.0
tags:
- video-generation
- music-video
- parody
- comedy
- wubuwizard
- waefrebeorn
- ai-training
- video-editing
pretty_name: WuBuWizard Cleveland Video Dataset (Parody)
