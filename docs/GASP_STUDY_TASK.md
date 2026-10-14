# Phase 0 — GASP Study Task (read-only)

**You have Monolith connected: you can read `.uasset` contents. Use it.**

This task produces **one document and zero edits**. Do not modify any asset, blueprint,
config or source file. If you believe an edit is required to answer a question, write the
question down as unanswered instead.

Read `docs/PARKOUR_HANDOFF.md` first — it holds the locked architecture decisions. This
file is the study that must happen before any of it is implemented.

---

## Mission

Answer one question: **what exactly does this project already have, and what is the
cheapest correct way to add four new traversal layers on top of it without breaking
GASP?**

The four layers, in the owner's priority order for *study* (build order differs):

| Layer | Nature | Includes |
|---|---|---|
| **L1 Ledges** | **State** — player hangs, waits, then decides | catch, hang idle loop, shimmy L/R, inside/outside corner, climb-up, drop, ledge-to-ledge jump |
| **L2 Wall run** | **State** — continuous contact | entry, loop (horizontal + vertical), eject, transition to L1 |
| **L3 Beam / balance** | **Path** — locked to a curve | mount, walk loop, turn, stop, dismount, fall-off |
| **L4 Rope, bar, swing** | **Path** (+ pendulum) | tightrope, monkey bars, hanging bar, rope swing, release into L1 or falling |

L3 and L4 are assumed to be one system (spline-locked movement, with a pendulum term for
swinging). **Confirm or refute that assumption** — it is the single biggest cost question
in the plan.

The wider goal is Split Fiction / It Takes Two style variety: many bespoke movement
mechanics, each appearing for one chapter. So a secondary question runs through everything
below: **how cheaply can a new movement state be added once the first one exists?**

---

## Part A — What GASP actually is here

Report, with asset paths and what you actually read (not what the docs say):

1. **Character chain.** GameMode default pawn → character class → parent. Is it the GASP
   sandbox character, a child, or an edited copy? Is the movement component stock
   `UCharacterMovementComponent` or a subclass? Current `MaxStepHeight`, capsule size,
   `bCanWalkOffLedges`, gravity scale, max speeds per gait.
2. **The traversal path, end to end.** Trace the actual call flow from input to montage:
   which function starts it, what geometry it measures, how it decides, how the montage is
   selected, how warp targets are set, how it ends. Name every asset involved.
3. **Does traversal depend on components placed on the obstacle** (spline components on a
   traversable actor), or is it pure trace-based? This decides whether every obstacle in
   the game needs authoring. State the answer plainly.
4. **The animation selection layer.** Which chooser table feeds pose search; how many pose
   search databases; what the state inputs to the chooser are; where `MovingTraversal` and
   `JustTraversed` are used; where the Default Slot montage injection happens; which warping
   nodes are in the graph and in what order; whether Offset Root Bone is enabled.
5. **Split screen and input.** How local players are created, how many input mapping
   contexts exist and who owns them, and any place the code or a blueprint reaches for
   player 0.
6. **Plugins enabled** in `.uproject`, and which of them the character actually uses.

## Part B — Modifiability assessment

For each of the following, answer **yes / no / conditional** with the reason and the cost:

1. Can the **high-obstacle** traversal actions be bypassed (routed into a custom state)
   while **low-obstacle** actions keep working untouched? The owner's decision: obstacles
   below a per-character threshold (~chest height, stored in a data asset) stay automatic;
   above it, the character hangs and waits for input. Can the split be made **without
   editing any GASP asset in place**?
2. Can the catch portion of an existing high-obstacle montage be reused while discarding
   its automatic climb-up tail? If yes, where is the cut point and what curves/notifies sit
   near it?
3. Can a custom movement mode coexist with the AnimBP without editing the AnimBP — i.e. can
   a gameplay tag or state enum reach the chooser through an existing input?
4. What happens to the **trajectory/prediction** component while the character is in a
   custom mode with zero CMC velocity? Name the component and the concrete failure.
5. Is there anything in the graph that assumes locomotion is always active — something that
   would visibly break during a long stationary hang?
6. **State template question:** after the first custom state exists, what is the per-state
   cost of adding the next one? List every place that must be touched (movement component,
   chooser, pose search, tags, input, data asset). If that list is longer than five places,
   propose how to shorten it — that proposal is the most valuable output of this study.

## Part C — Animation inventory and gap list

Using Monolith, **enumerate what actually exists**, not what the docs claim:

1. Every traversal-related animation asset in the project, with name, whether it has root
   motion, its duration, and the notifies/curves on it. Group by action type.
2. Is there **any** hanging content: a hang idle, shimmy, corner, or ledge-to-ledge clip?
   State clearly if the answer is none.
3. For each of L1–L4, list the **missing clips** with a count and a one-line description
   each. Mark which are strictly required for a first playable version and which are polish.
4. For each missing clip, state whether it can be **derived** from something present
   (mirrored, retimed, reused as a blend-in) or must be authored/purchased.
5. Report the **reference offsets** of existing clips where measurable: hand-to-wall
   distance for anything wall-adjacent, obstacle depth for vaults. Any clip that will be
   authored later must match these numbers, so they must be written down now.

## Part D — Acceleration options

For each layer, give a recommendation with a cost and a risk:

- **Build** — what we write; estimate in days.
- **Buy** — only name assets you can verify exist and what specifically they would replace.
  For any purchase, state whether it ships animations, whether it is a plugin or a whole
  project, and whether its licence is explicit about commercial use of **both code and
  animations**. Flag anything distributed outside an official marketplace.
- **Derive** — what can be produced from existing GASP content for free.

Then answer directly: **which single purchase, if any, saves the most time for this
specific project?** The owner authors their own clips and has limited budget. A "no
purchase needed" answer is a valid and welcome result.

## Part E — Build order proposal

Propose the order to implement L1–L4, with a hard gate after the first one. The owner's
current preference is L3 (beam) first because it is a path system with no detection and no
physics, proving the spline component that two layers share, and showing a visible result
in days. **Agree or disagree with a reason** — do not simply comply.

For the first layer only, propose the concrete file list you would create: class names,
what each owns, and which existing asset each one talks to. No implementation.

---

## Constraints

- **Read-only.** No asset, blueprint, config or source edit. Commit the repo before you
  start so any accidental change is visible.
- **Never edit GASP assets in place** in any later phase; note anywhere the current project
  already has.
- Do not propose replacing the AnimBP, motion matching, or the chooser.
- Do not propose putting gameplay decisions in Blueprint, the Level Blueprint, or the
  AnimBP. Logic goes in C++; blueprints call and wire.
- Do not guess numbers. Every figure must be read from the project, with the asset path
  where you read it.
- Do not introduce any dependency on player index 0 in your proposals. Two local players,
  split screen.

## Deliverable — `docs/PHASE0_REPORT.md`

Structure it exactly as Parts A–E above, then add:

1. **Blockers** — anything that makes the plan in `PARKOUR_HANDOFF.md` wrong or impossible,
   stated bluntly. This section matters more than the rest.
2. **Decisions taken without instruction**, each with the alternatives you rejected.
3. **Confidence table** — every claim you are unsure about, rated low/medium/high, with
   what would settle it. Low confidence is expected and must be recorded, not hidden.
4. **First five things the owner should test in the editor**, ranked by risk.

Keep it factual. Asset paths over adjectives. If a question cannot be answered with
read-only access, say so and say what access would answer it.
