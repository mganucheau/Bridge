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
| `00-design-system.svg` | Colour tokens, typography scale, and component atoms (knobs, buttons, selectors, tab strip). One language for all five panels. |
| `01-leader-redesign.svg` | The **Leader** (main band-leader) view — header, conductor knobs, and the band mixer. |
| `02-animal-redesign.svg` | The **Animal** drum panel — drum grid with lane M/S, grouped groove + dynamics knobs, actions. |
| `03-bootsy-redesign.svg` | The **Bootsy** bass panel — pitch pickers, piano-roll note grid, groove + expression knobs, style chips, actions. |
| `04-stevie-redesign.svg` | The **Stevie** piano panel — same layout as Bootsy to prove cross-page uniformity, different accent colour and chord-stack note data. |
| `05-paul-redesign.svg` | The **Paul** guitar panel — identical layout to Bootsy / Stevie with a fretboard sidebar instead of a piano keyboard. |

## Problems this redesign addresses

The user called out three things: flat / unbalanced layout, no cross-page
uniformity, and "functions that shouldn't be there." After reading every panel
file, here's what is actually broken today:

1. **Colour identity is inconsistent.** The tab strip in `BridgeEditor.cpp`
   assigns a different colour per instrument (`cLeader 0xffd4a84b`,
   `cAnimal 0xffe07a5a`, `cBootsy 0xff5cb8a8`, `cStevie 0xffb88cff`,
   `cPaul 0xff6eb3ff`), but three of the four panel LookAndFeels (`BootsyM3`,
   `StevieM3`, `PaulM3`) all use the same amber `#FFB84D`, and `AnimalM3`
   uses purple `#D0BCFF` — none of which match their own tab colour.
   **Fix:** one accent colour per instrument, flowing from tab chip → title
   underline → section headings → knob rings → primary action button.
2. **A "visual only" ticker speed control.** Each instrument panel has a
   `Speed` button cluster (`x2 / 1 / 1/2`) whose tooltip literally reads
   *"(visual only)"*. It doesn't affect audio output — only the playhead
   animation. For an audio plugin that's dead weight and a UI trap. **Fix:**
   removed from every redesigned panel. (The `tickerSpeed` parameter can stay
   in the APVTS for state compatibility, it just shouldn't have a UI.)
3. **Unbalanced knob layouts.** The Leader panel arranges 5 knobs as 3 + 2
   rows; Animal has 8 as 2 rows of 4 (good); Bootsy/Stevie/Paul have 9 as an
   awkward 5 + 4. Each instrument panel also shares a single cramped "loop row"
   with six unrelated things (loop start, loop end, lock, speed, gen, fill).
   **Fix:**
   - Leader: 5 knobs in one balanced row.
   - Animal: 4 + 4 grouped into **GROOVE** and **DYNAMICS** sections.
   - Bootsy / Stevie / Paul: 5 + 4 grouped into **GROOVE** and **EXPRESSION**
     sections — same grid, same knob positions across all three panels.
4. **No unified header.** The current editor just has a 44 px tab strip with
   tiny power dots. There is no plugin identity, no transport state, nowhere
   to show BPM or host-sync.
   **Fix:** a 60 px header bar on every page with the `BRIDGE` wordmark,
   BPM readout, play/stop/sync indicators, and the instrument tab chips
   (keeping the existing power-toggle behaviour, just styled as chips with
   a coloured dot + label and a filled active state).
5. **No grouping, no section affordance.** The current panels drop all their
   controls directly into the panel area with no visible cards, dividers, or
   labels — everything looks like it's at the same level of importance.
   **Fix:** every content area is a `#211E29` card with a `14 px` radius, a
   soft drop shadow, a coloured title rule, an 11 px 700-weight letter-spaced
   section label, and a grey helper subtitle. This gives the eye clear "zones"
   to scan.
6. **Loop + actions are crammed together.** The loop start/end/lock, the
   (now removed) speed ticker, and the GEN / PERF / FILL buttons all
   currently share a single horizontal row.
   **Fix:** loop range is demoted to a compact pill shown in the top-right of
   the grid card ("LOOP 1 ─── 16 🔒"), freeing the bottom of the panel for a
   dedicated **STYLE** + **ACTIONS** column. Generate is promoted to a large
   primary-colour button — it's the headline action of an AI generator.

## Design system at a glance

### Colour per instrument

| Instrument | Accent | Hex | Used for |
| --- | --- | --- | --- |
| Leader   | warm gold   | `#D4A84B` | conductor knobs, title rule, style pill |
| Animal   | coral       | `#E07A5A` | drum cells, groove knobs, GENERATE button |
| Bootsy   | teal        | `#5CB8A8` | bass notes, knobs, GENERATE button |
| Stevie   | violet      | `#B88CFF` | piano notes, knobs, GENERATE button |
| Paul     | sky blue    | `#6EB3FF` | guitar notes, knobs, GENERATE button |

Each panel also gets a subtle one-directional accent-tinted glow
(`fill-opacity ≈ 0.16`) across the grid card so the instrument colour is
visible even at a glance.

### Neutral palette

```
app bg   #14121A   (existing MainPanel bg, retained)
panel    #1A1820   (existing BridgeEditor bg, retained)
card     #211E29
card hi  #2A2632
stroke   #3A3548
text pri #EAE2D5
text sec #B0A8C4
text ter #6A6578
```

### Spacing & geometry

- 8 px baseline grid.
- 24 px outer margin on every panel.
- 14 px card radius, 10 px button radius, 6 px small-control radius.
- 72 px knob diameter (was ~86 in Leader and ~64 in the instrument panels —
  now one size everywhere).
- 44 px standard button height, 52 px for the primary `GENERATE`.

### Type scale

- Panel title — 26 / 700 / +1 letter-spacing.
- Section label — 11 / 700 / +2 letter-spacing, coloured with the instrument
  accent.
- Knob label — 11 / 600 / 0 letter-spacing, `#B0A8C4`.
- Helper / metadata — 9 / 400 / +1.5 letter-spacing, `#6A6578`.
- Instrument name (in mixer row) — 16 / 700 / 0 letter-spacing.

## Panel-by-panel notes

### Leader (`01-leader-redesign.svg`)

- **Header** — BRIDGE wordmark (with gold bar matching the active tab), BPM
  `120`, play / stop, `HOST SYNC` indicator, and five tab chips. The active
  chip is filled with its accent colour.
- **Conductor card** — one balanced row of 5 gold knobs (Presence, Tight,
  Unity, Breath, Spark). The style combo ("Soul Classic · Deep Pocket") lives
  in the top-right of the same card instead of floating above.
- **Band mixer** — one card per instrument, each with:
  - a 4 px accent bar on the left edge,
  - the instrument name + short subtitle ("Drums · 16 steps · 8 lanes",
    "Bass · C Mixolydian · Oct 2", etc.),
  - the existing `StripPreview` visualisation at 5x the current size (so you
    can actually read the pattern),
  - M / S pills,
  - an `OPEN →` button to jump to that instrument's tab.

### Animal (`02-animal-redesign.svg`)

- **Drum grid card** — 8 lanes × 16 steps with lane labels + M / S buttons in
  a fixed left gutter. The grid itself shades every 4-step block slightly
  differently so beats are readable. Active hits use velocity-modulated alpha.
  A playhead line runs the full height.
- The loop range and lock button move out of the bottom row into a compact
  pill in the card header.
- **Groove card** — 4 knobs (Density / Swing / Humanize / Pocket) above
  4 knobs (Velocity / Ghost / Complexity / Fill Rate). Dividing rule
  between the two sub-sections.
- **Style & actions card** — style selector up top, a large coral primary
  `GENERATE` button, `PERFORM` (toggle, outlined) and `FILL ◉ HOLD` on the
  row below. Helper text explains what each action does.

### Bootsy / Stevie / Paul (`03`, `04`, `05`)

Identical structural template — that's the point. The only differences are:

- Accent colour (teal / violet / sky).
- The sidebar next to the note grid: piano keyboard for Bootsy and Stevie,
  fretboard with string labels for Paul.
- Note-data shape: Bootsy's is a single rolling bass line, Stevie shows a
  melody with a stacked chord voicing underneath, Paul shows riffs on the top
  strings + chord hits on the bottom strings.
- Style chip copy.

Everything else — header, pitch row, grid card position, knob grid,
action card — is placed on the same coordinates. Tabbing between these
three instruments should feel like the same tool, not three different
plugins.

## Out of scope / worth discussing separately

- **`PERFORM` only exists on Animal.** Bootsy / Stevie / Paul don't have a
  perform button in the current code, so I didn't invent one. If Perform is
  intended to be a universal feature, all four panels should get it in the
  same position.
- **Dynamic style button rows.** The current Bootsy/Stevie/Paul code builds a
  row of `NUM_STYLES` toggle buttons at runtime. I've mocked them as a 2 × 3
  grid of chips with hand-picked names (FUNK / SOUL / ...); adjust to the real
  style list per engine.
- **Per-lane drum M/S currently lives on `apvtsAnimal`.** The mockup keeps it
  exactly where it is conceptually (in the left gutter of the drum grid),
  just bigger and more readable.
- **Power toggles.** The existing `powerLeader / powerAnimal / ...` buttons
  map to the small coloured dot at the left of each tab chip. Clicking the
  dot still toggles power; clicking the label still switches tab. Same
  behaviour, cleaner affordance.
