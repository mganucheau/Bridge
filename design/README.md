# Bridge — GUI Redesign Mockups

A set of SVG mockups proposing a polished, unified visual language for the
Bridge AI band-generator plugin. Every mockup is rendered at the plugin's
native window size (960 × 740) so it can be compared 1:1 with the current
editor.

Open any `.svg` file in a browser, image viewer, or Figma — they are
self-contained vector files with no external dependencies.

## Files

| File | What it shows |
| --- | --- |
| `00-design-system.svg` | Colour tokens, typography scale, and component atoms (knobs, buttons, selectors, tab chip, page-title + on/off toggle). |
| `01-leader-redesign.svg` | The **Leader** view — Band mixer on top, knobs + loop/style/actions on the bottom. |
| `02-animal-redesign.svg` | The **Drums** (Animal) panel — kick at bottom, in-grid loop brackets, 8-knob row + shared bottom card. |
| `03-bootsy-redesign.svg` | The **Bass** (Bootsy) panel — piano sidebar + note grid, 9-knob row + shared bottom card. |
| `04-stevie-redesign.svg` | The **Keys** (Stevie) panel — same template as Bass, chord-stack voicings. |
| `05-paul-redesign.svg` | The **Guitar** (Paul) panel — fretboard dropped, piano sidebar + note grid just like Bass/Keys. |

## v2 — what changed after the Phase 2 review

This pass applies the user's follow-up brief on top of the merged `main`
(commit `e70ea6e`, which introduced `BridgeInstrumentStyles.h` and
`MelodicGridLayout.h` for shared layout helpers and renamed the tab labels
from `Animal/Bootsy/Stevie/Paul` → `Drums/Bass/Keys/Guitar`).

### Leader page
- **Band mixer is now at the top** (replaces the old grid area), knobs + the
  loop/style/actions card are at the bottom — so the Leader's bottom half
  matches every instrument page.
- **Style / Key / Root pickers live on the same row as the "Band" title**
  inside the mixer card header. The per-row description text
  (`# of instruments`, `key`, `octave`) has been removed.
- **Each mixer row is now the same overall size as an instrument grid
  section**, and each row is a tiny live replica of that instrument's grid
  (drum cells for Drums; one-octave note scatter for Bass / Keys / Guitar).
- The loop + style + actions card sits next to the knob section, just like
  on every other instrument.

### Every page (unified bottom half)
- **Bottom half is identical on every window**: one horizontal row of
  knobs on the left, one `Loop / Style / Actions` card on the right. Only
  the knob labels and accent colour change per instrument.
- Inside the bottom card, in vertical order:
  1. Loop Start slider + 🔒 lock, Loop End slider + 🔒 lock, Speed x2 / 1 / 1/2
  2. Style dropdown
  3. `GEN` / `FILL` / `PERF` action buttons — no helper text underneath.
- Section sub-titles above the knob rows (`Time & feel`,
  `Velocity & expression`, …) are **removed** — the knobs are labelled
  individually and that's enough.

### Tabs, titles and toggles
- **Power dot is removed from each tab chip.** The top tab strip is now
  clean text chips; the active chip is distinguished by its instrument
  accent border + bottom rule.
- **An on/off toggle sits to the LEFT of each page title** (a small green
  pill-switch), replacing the old power dot that was overflowing the tab
  chip. Clicking it still toggles the instrument on/off — same parameter,
  cleaner home.
- **Page title is top-padded** so it's no longer flush with the panel
  border — it gets the same vertical breathing room as every other section.
- **Page sub-titles are gone from under the title.** Their content (the
  `8 LANES · 16 STEPS · …` descriptor) moves to the same row as
  `Root / Scale / Oct`, right-aligned, centred to the dropdown baseline.

### Dropdown row (non-Leader)
- `Root`, `Scale` and `Oct` labels are the same size as the dropdown text
  and vertically centred with the dropdown baseline — no more off-by-a-pixel
  label stack.
- `Style` dropdown is no longer on this row — it lives inside the bottom
  loop/style/actions card.

### Grid cards
- **In-grid loop selectors replace the old "loop slider above the grid"
  pill.** A coloured bracket now draws on the grid at the Start and End
  columns with a small `START · N` / `END · N` hint above each bracket.
- **Drums: kick is at the bottom of the grid** (lanes reversed); the piano
  sidebar labels follow the new order.
- **Drum grid has matching vertical padding above and below** to mirror the
  Bass/Keys/Guitar piano-roll card — Animal no longer feels cramped while
  the melodic engines breathe.
- **Paul no longer uses a fretboard sidebar.** Guitar now uses the same
  `piano sidebar + note grid` layout as Bass and Keys so tabbing between the
  three melodic instruments feels like one tool.

## Problems this redesign addresses

The user's original brief: flat / unbalanced layout, no cross-page
uniformity, and "functions that shouldn't be there." After reading every
panel file and the merged-main helpers, here's what is actually broken
today:

1. **Colour identity is inconsistent.** The tab strip in `BridgeEditor.cpp`
   assigns a different surface per instrument, but three of the four panel
   LookAndFeels (`BootsyM3`, `StevieM3`, `PaulM3`) share the same amber
   `#FFB84D` while `AnimalM3` uses purple `#D0BCFF` — none of which match
   their own tab colour.
   **Fix:** one accent per instrument, flowing from tab chip → title rule
   → knob rings → primary action button.
2. **A "visual only" ticker speed control.** Each panel has an `x2 / 1 / 1/2`
   cluster whose tooltip literally reads *"(visual only)"*. For an audio
   plugin that's dead weight.
   **Fix:** kept in the design for parameter-state compatibility but
   demoted to a small inline control inside the loop/style/actions card —
   no longer a standalone section.
3. **Unbalanced knob layouts.** Animal has 8 knobs, Bootsy/Stevie/Paul have
   9; the current code splits them awkwardly across sub-rows.
   **Fix:** one horizontal row, per-instrument, labelled per-knob, no
   sub-section headings.
4. **Loop + actions crammed together.** Loop start/end, lock, speed and
   GEN / FILL / PERF all share one cramped horizontal row in the existing
   panels.
   **Fix:** a single dedicated card at the bottom-right stacks them
   vertically (loop → style → actions), identical on every page.
5. **No grouping, no card affordance.** The current panels drop controls
   into the panel with no visible cards or dividers.
   **Fix:** every content area is a rounded card with a soft drop-shadow
   and an instrument accent rule so the eye gets clear zones.
6. **Tab chip overflow.** The power dot packed inside each 168 × 36 tab
   chip visibly overflows the label on the current build.
   **Fix:** remove the dot from the chip; the on/off toggle moves to the
   left of the page title.

## Design system at a glance

### Colour per instrument

| Instrument | Accent | Hex | Used for |
| --- | --- | --- | --- |
| Leader (Band)  | warm gold   | `#D4A84B` | knobs, title rule, actions |
| Drums (Animal) | coral       | `#E07A5A` | drum cells, knobs, GEN |
| Bass (Bootsy)  | teal        | `#5CB8A8` | bass notes, knobs, GEN |
| Keys (Stevie)  | violet      | `#B88CFF` | chord notes, knobs, GEN |
| Guitar (Paul)  | sky blue    | `#6EB3FF` | guitar notes, knobs, GEN |

### Neutral palette

```
app bg   #14121A
card     #23202B → #1B1823 (gradient)
stroke   #3A3548 / #2E2937
text pri #EAE2D5
text sec #B0A8C4
text ter #6A6578
```

### Spacing & geometry

- 8 px baseline grid.
- 24 px outer margin on every panel.
- 14 px card radius, 10 px button radius, 6 px small-control radius.
- One knob diameter per section (72 px on Leader; 58–64 px on instruments
  to fit 8–9 knobs across the same row width).
- `kKnobRowH = 86`, `kLoopRowH = 100` (matches `bridge::instrumentLayout::*`
  from merged main).

### Type scale

- Page title — 22 / 700 / +3 letter-spacing.
- Section label — 11–13 / 700 / +2 letter-spacing, coloured with the
  instrument accent.
- Knob label — 10 / 0 letter-spacing, `#B0A8C4`.
- Dropdown label — 12 / 400, same size as the dropdown text, baseline
  aligned.
- Helper / metadata — 9–11 / +1.5 letter-spacing, `#6A6578`.

## Out of scope / worth discussing separately

- **Global Key / Root on Leader.** The mockup proposes `Style / Key / Root`
  dropdowns on the Band card header as a new global control set for the
  whole band. Today only `leaderStyle` exists in `apvtsMain`; a `leaderRoot`
  and `leaderScale` would need to be added to wire this up.
- **Two loop lock buttons.** The current code uses one `loopWidthLock` that
  links Start and End. The mockup shows a separate lock next to each knob to
  match the brief's "Loop Start lock button and End button like it currently
  has" wording. This is a code change: either split into `loopStartLock` /
  `loopEndLock` or re-interpret both UI buttons as the same underlying
  parameter.
- **Dynamic style button rows.** Bootsy/Stevie/Paul currently build a
  `NUM_STYLES` style-button row at runtime. The mockup replaces it with a
  single dropdown inside the bottom card — simpler, same data source.
- **PERFORM on every instrument.** Animal has `perform`, the melodic engines
  don't. The mockup shows PERF on every page for layout uniformity; the
  button can be disabled on engines that don't implement it yet.
