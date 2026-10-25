[简体中文](design-sources.zh_CN.md)

# Interface sources and decisions

The existing vanilla JavaScript / Python standard-library application is retained.
The design uses one self-hosted Noto Sans SC family, white surfaces, dark text and
blue actions. There are no CDN requests, decorative display fonts or new backend framework.

| Source | Actual use | License |
| --- | --- | --- |
| shadcn/ui | Button variant and visible-focus conventions adapted to existing CSS | MIT |
| Motion 14.6.0 | Local browser bundle for spring entrances, keyed-row layout transitions and dialog exits | MIT |
| Lucide Static 1.61.0 | Locally generated SVG symbol sprite | ISC |
| LVGL 9.5.0 | Existing firmware renderer, retained without a version upgrade | MIT |

[shadcn/ui source](https://github.com/shadcn-ui/ui)

[Motion source](https://github.com/motiondivision/motion)

[Lucide source](https://github.com/lucide-icons/lucide)

Beautiful UI, BeUI, Rare UI and Transitions were reviewed. Their framework-specific
components were not added to this vanilla application. Transitions has additional
snippet redistribution restrictions, so no snippet was copied from it.

Local skills used: find-skills, black-white-ui-prototyping, web-design-guidelines,
create-readme, passport-develop and passport-build. Skills are implementation guidance,
not shipped dependencies. User preferences override generic typography suggestions.

Reduced-motion preference removes movement. Animation never postpones button state
changes; dialog exits last 120 ms. Long actor lists render lazily and stagger only the
first ten entries to avoid a long wait. Firmware uses immediate selection feedback,
without allocating screen-sized animation buffers on the ESP32-C3.

Exact package download URLs and SHA-256 values are in
`website/static/vendor/versions.json`; vendor licenses are stored alongside the code.
