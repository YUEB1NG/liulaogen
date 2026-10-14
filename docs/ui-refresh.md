[简体中文](ui-refresh.zh_CN.md)

# Website interface refresh

The public schedule and administrator workspace share the existing Python and vanilla HTML/CSS/JavaScript application. No framework is introduced; fonts are served by the website without a third-party font service. Firmware and device protocols are unchanged.

## Visual direction

The compact scheduling workspace retains charcoal surfaces and blue actions. Serif headings, ornamental lettering, slogans and step numbers are removed. Headings, body copy, numbers and controls use the uploaded Qiaomu Noto Sans SC family, with hierarchy expressed through size, weight and spacing. Initial font transfer is approximately 2.44 MiB, cached for seven days; text remains readable while fonts load.

The implementation follows Salin UI layout/component guidance and Impeccable Design Polish after reviewing descriptions of 308 installed skills. Desktop uses a filter column beside the roster; screens at or below 768px use one column. Spacing is 8–12px within groups and 16–24px between sections. Mobile targets are at least 44px high, inputs use 16px text, and the primary publish action is distinct from save and device actions.

Regular and bold files are unchanged from the uploaded `qiaomu-cover-fonts` package, with their OFL licenses. Three characters in historical actor data use a subset of the same Noto Sans CJK SC family from the uploaded `koharu` package, renamed Liulaogen Sans Extras. Exact allowlisted routes serve fonts. Only fonts are cached; account and content APIs remain `no-store`.

Desktop uses a date/tools column beside the lineup. Mobile keeps a single lineup column, aligned date controls and compact management shortcuts. Actor forms, search results, previews, city settings and device cards share typography, spacing and control states. Pairing details collapse when opening a populated device list but remain open during refresh if the user expands them.

## Public and administrator views

Visitors read only published lineups from the existing public API. Empty dates or sessions are explicit; network/server errors remain errors and do not become empty schedules. Published session times come from the frozen publication. Administrator drafts remain behind login. Save, publish and device upload remain separate operations; existing confirmations remain in place.

Dialogs have accessible names; controls have focus/pressed/disabled states. A skip link supports keyboard navigation. Loading work restores focus when appropriate; reduced-motion preferences disable animation. The actor picker still does not autofocus search. Storage details can expand and collapse.

## Validation

Website: 58 backend tests passed, including font responses, cache boundaries and path restrictions. Both weight stacks cover all 1,594 distinct CJK characters in UI strings and bundled data. Browser checks inspect actual rendered fonts for headings, body, controls and the supplemental characters. Edge Chromium touch emulation passed at 320, 390, 768 and 1280 pixels, covering date alignment, actor and alias search, sources, city/time editing, device selection/rename/upload/delete, logout confirmation and storage controls. Added browser checks cover published-vs-draft isolation, missing schedules, request failures and pairing input surviving list refresh. Screenshots were inspected for public, administrator, actor and device views. Real iPhone Safari remains untested.
