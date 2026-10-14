[简体中文](ui-refresh.zh_CN.md)

# Website interface refresh

The public schedule and administrator workspace share the existing Python and vanilla HTML/CSS/JavaScript application. No framework or external font dependency is introduced. Firmware and device protocols are unchanged.

## Visual direction

Charcoal surfaces, restrained blue actions, warm editorial headings and a compact stage-inspired wordmark preserve the established dark interface. Shared CSS tokens define text, surfaces, separators, focus and action colors. Local Chinese font fallbacks avoid network font requests.

Desktop uses a date/tools column beside the lineup. Mobile keeps a single lineup column, aligned date controls and compact management shortcuts. Actor forms, search results, previews, city settings and device cards share typography, spacing and control states. Pairing details collapse when opening a populated device list but remain open during refresh if the user expands them.

## Public and administrator views

Visitors read only published lineups from the existing public API. Empty dates or sessions are explicit; network/server errors remain errors and do not become empty schedules. Published session times come from the frozen publication. Administrator drafts remain behind login. Save, publish and device upload remain separate operations; existing confirmations remain in place.

Dialogs have accessible names; controls have focus/pressed/disabled states. A skip link supports keyboard navigation. Loading work restores focus when appropriate; reduced-motion preferences disable animation. The actor picker still does not autofocus search. Storage details can expand and collapse.

## Validation

Website: 56 backend tests passed. Edge Chromium touch emulation passed at 320, 390, 768 and 1280 pixels, covering date alignment, actor and alias search, sources, city/time editing, device selection/rename/upload/delete, logout confirmation and storage controls. Added browser checks cover published-vs-draft isolation, missing schedules, request failures and pairing input surviving list refresh. Screenshots were inspected for public, administrator, actor and device views. Real iPhone Safari remains untested.
