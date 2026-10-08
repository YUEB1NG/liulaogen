<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

The cast application uses `fonts/NotoSansCJKsc-Cast.otf` (Noto Sans CJK SC,
SIL Open Font License in `fonts/OFL.txt`) and the shipped `fonts/cast_font_16.c`:
16 px, 4 bpp, uncompressed, 24 px line height, 6900 code points. The subset,
inventory and generated C hashes are recorded in `fonts/cast-font-manifest.json`.
All application labels select `cast_font_16` explicitly. The original assets
were generated with Pillow 11.0.0 / FreeType 2.13.3; the Wi-Fi continuation uses
the same rasterizer with fonttools 4.55.3 to add GB2312 common Han characters.
The source is [Noto Sans CJK SC Regular, Sans2.004](https://github.com/notofonts/noto-cjk/blob/Sans2.004/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf),
SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.
Run `python tools/generate_cast.py`, `python tools/generate_cast_supported.py`,
and `python tools/generate_cast_font.py <source.otf>` to reproduce the subset.
Run `python tools/check_cast_font.py` to verify hashes, cmap,
bitmap bounds, supported network text and widget bindings. The gate also runs
`python tools/generate_cast.py --check`. Unknown network glyphs reject the
entire update while preserving the previous snapshot. Real display rendering
and scrolling still require device acceptance.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

The r5 supplemental inventory in `fonts/catalog-characters.txt` covers the user-supplied October 8 actor names, credits and biographies; its source wording is maintained in the website actor catalog.
