# PHASE 0 — GASP Study

Read-only. No asset, blueprint, config or source file was modified. Baseline commit
`8cf4cd0` was made before the study began; `git status --untracked-files=no` was clean at
that point, so any change is visible in the diff.

Everything below was read through Monolith (live editor, port 9316) or from the files
themselves. Where I did not read something, it is in the confidence table, not in the prose.

---

# Part A — What GASP actually is here

## A1 · Character chain

| Link | Value | Read from |
|---|---|---|
| GameMode | `/Game/_Project/Core/GM_Phase0` (parent `GM_Sandbox_C`) | CDO |
| `DefaultPawnClass` | **`/Game/Blueprints/BP_L2H_Pawn_Phase1`** | CDO |
| `PlayerControllerClass` | `/Game/_Project/Core/PC_L2H` | CDO |
| `BP_L2H_Pawn_Phase1` parent | **`SandboxCharacter_Mover_C`** | `get_parent_class` |
| `BP_L2H_Pawn_Phase1` kind | `is_data_only: true` — property overrides only, no graph | `get_parent_class` |
| `SandboxCharacter_CMC` parent | **`/Script/Engine.Character`** — stock | `get_parent_class` |
| Movement component | **stock `UCharacterMovementComponent`**, native subobject `CharMoveComp` | `get_components` |

**GASP assets are not edited in place.** The project's pawn is a child, and both GASP
sandbox characters are untouched. `GM_Sandbox` even ships a runtime pawn switcher —
`PawnClasses_Soft = [SandboxCharacter_CMC, SandboxCharacter_Mover]`.

**But the live default pawn is the Mover one.** See Blockers §1.

### Numbers — `/Game/Blueprints/SandboxCharacter_CMC` CDO

| Property | Value |
|---|---|
| `CapsuleHalfHeight` / `CapsuleRadius` | **86 / 30** |
| `MaxStepHeight` | **45** |
| `bCanWalkOffLedges` | `True` |
| `GravityScale` | 1.0 |
| `JumpZVelocity` | 500 |
| `MaxWalkSpeed` / `Crouched` | 500 / 300 |
| `MaxAcceleration` / `MinAnalogWalkSpeed` | 500 / 150 |
| `MaxCustomMovementSpeed` | **600** |
| `CrouchedHalfHeight` | 60 |
| `WalkableFloorAngle` / `Z` | 44.765 / 0.71 |
| `PerchRadiusThreshold` / `PerchAdditionalHeight` | 20 / 40 |
| `LedgeCheckThreshold` | 4 |
| `bOrientRotationToMovement` / `bUseControllerDesiredRotation` | `False` / `True` |
| `RotationRate` | Yaw 360 |
| `bUseFlatBaseForFloorChecks` | `True` |
| Capsule `BodyInstance` | `ObjectType=ECC_Pawn`, profile `Custom`, **ignores Visibility, Pawn, Camera, PhysicsBody** |

`MaxWalkSpeed 500` is the CDO value; GASP drives gait speeds at runtime, so this is the
floor not the truth. Per-gait speeds were not read — see confidence table.

> **Correction to existing code:** `LTHLedgeHangMode::FindStandSpot` falls back to 34/88 when
> the updated primitive is not a capsule. The real capsule is **30/86**. It reads the live
> capsule when present, so this is a fallback-only error, but it is wrong.

## A2 · The traversal path, end to end

Input `/Game/Input/IA_Traverse` → `SandboxCharacter_CMC::EventGraph` (two call sites,
`K2Node_CallFunction_80` and `_11`, under the comment *"Jump / Traversal Input"*) →
`GetTraversalCheckInputs` (a **function** on the character) → `AC_TraversalLogic::TryTraversalAction`.

`/Game/Blueprints/AC_TraversalLogic` exposes exactly five functions:
`TryTraversalAction`, `SetWarpTargets`, `SetMovementMode_CMC`, `GetEndMovementMode_CMC`,
`SetReplicationBehavior`.

Inside `TryTraversalAction` — Epic's own step comments, verbatim structure:

| Step | What it does |
|---|---|
| 2.1 | Forward trace → **`Cast To LevelBlock_Traversable`**; cast fails ⇒ exit |
| 2.2 | `GetLedgeTransforms` on the block — *"using its own internal function"* |
| 3.1 | Valid front ledge? else exit |
| 3.2 | Trace actor → front ledge: is there room to move up to it |
| 3.3 | **Obstacle height** = delta actor ↔ front ledge |
| 3.4 | Trace across the top, front ledge → back ledge |
| 3.5 | **Obstacle depth** = front↔back delta, or front↔impact if blocked (back ledge invalidated) |
| 3.6 | Trace down from back ledge → back floor, or invalidate |
| 4.1 | Send front ledge location **to the AnimBP through an interface** — becomes a custom channel in the following Motion Matching search |
| 4.2 | `Evaluate Chooser: CHT_TraversalMontages_CMC` with a PoseMatch column → montage **and entry frame** from distance-to-ledge + current pose |
| 5.5 | `PerformTraversalActionCMC` plays it |

`SetWarpTargets` sets `FrontLedge` always; sets `BackLedge` only for hurdle/vault and
`RemoveWarpTarget`s it otherwise; sets `BackFloor`. It reads the curve
**`Distance_From_Ledge`** via `GetCurveValueFromAnimation` at the end of each warp window.

Epic's comment on why, verbatim: *"Because the traversal animations move at different
distances (no fixed metrics), we need to know how far the animation moves in order to warp
it properly"* — and *"therefore having fixed metrics for all traversal animations would be
an improvement."*

Movement mode is set by the component itself: `SetMovementMode_CMC(EMovementMode)` and
`GetEndMovementMode_CMC()`.

## A3 · Does traversal depend on components on the obstacle?

**Yes. Plainly: every traversable obstacle must be a `LevelBlock_Traversable` with authored
ledge splines.**

`/Game/Levels/LevelPrototyping/LevelBlock_Traversable` carries four `SplineComponent`s:
`Ledge_1`, `Ledge_2`, `Ledge_3`, `Ledge_4`. Step 2.1 traces forward and **casts** to that
class; a failed cast exits the function. The trace only *finds* the actor — all geometry
comes from the splines via `GetLedgeTransforms`.

This is a hybrid, not a trace-based system. Consequences:

- GASP traversal will **never** fire on ordinary static meshes. It is inert on arbitrary level geometry.
- A Split Fiction-style game of bespoke one-chapter geometry would need four splines authored per obstacle.
- **Conversely:** the spline-query pattern L3 and L4 need is already in the project, shipped by Epic, and working.

## A4 · The animation selection layer

**Chooser.** `/Game/Characters/UEFN_Mannequin/Animations/Traversal/CHT_TraversalMontages_CMC`
(a `_Mover` twin sits beside it). **Six columns, four rows:**

| # | Type | Bound to | Row values |
|---|---|---|---|
| 0 | Bool | `HasFrontLedge` | True · True · True · True |
| 1 | Bool | `HasBackLedge` | True · True · True · Any |
| 2 | Bool | `HasBackFloor` | True · True · **False** · Any |
| 3 | FloatRange | `ObstacleDepth` | 0–59 · 0–59 · 0–59 · **59–∞** |
| 4 | FloatRange | `BackLedgeHeight` | **50–∞** · 0–10 · any · any |
| 5 | **Output** Enum | `ActionType` (`/Game/Blueprints/Data/E_TraversalActionType`) | 1 · 3 · 2 · 3 |

All five inputs bind to `ContextIndex 1` (the traversal-check struct); the output enum binds
to `ContextIndex 2`.

**`ObstacleHeight` is computed in step 3.3 and is not a chooser column.** GASP's traversal
does not branch on height at all — it branches on depth and on what is behind the obstacle.
That is directly relevant to B1.

**Pose search.** Five database chooser tables — `CHT_PoseSearchDatabases` plus `_Dense`,
`_Sparse`, `_ExtremeSparse`, `_Relaxed`. At least 60 `PoseSearchDatabase` assets (the query
returned a full page of 60; exact total unread). Traversal has its own
`PSD_Traversal` + `PSS_Traversal`. The exits from traversal are separate databases:
`PSD_Dense_Stand_Walk_FromTraversal`, `PSD_Dense_Stand_Run_FromTraversal`,
`PSD_Dense_Jumps_FromTraversal`.

**Slot.** `AM_M_Neutral_Traversal_Catch_Cliff_high_standF` → `slots: [DefaultSlot]`,
duration **5.0 s**, blend in/out 0.25, skeleton `SK_UEFN_Mannequin`. Montage injection is
the Default Slot, as the handoff assumed.

**Not read:** where `MovingTraversal` and `JustTraversed` are consumed, the order of warping
nodes in the AnimBP, and whether Offset Root Bone is enabled. See confidence table.

## A5 · Split screen and input

**One input mapping context exists in the entire project: `/Game/Input/IMC_Sandbox`.**
`list_input_mapping_contexts` over `/Game` returns `total: 1`.

That is not automatically wrong — an IMC is added per local player through that player's own
`UEnhancedInputLocalPlayerSubsystem` — but it means there is no per-player context today, so
any per-character input divergence has to come from the action bindings, not the context.

Player-0 assumptions in C++: **zero.**
```
grep -E "PlayerCharacter\(0\)|PlayerController\(0\)|GetFirstPlayerController|GetPlayerPawn\(" Source/
→ no matches
```
The Blueprint side was **not** audited for the same pattern. See confidence table.

## A6 · Plugins

21 enabled in `lastTwoHours.uproject`: `ModelingToolsEditorMode`, `AnimationWarping`,
`RigLogic`, `LiveLink`, `LiveLinkControlRig`, `PoseSearch`, `AnimationLocomotionLibrary`,
`MotionWarping`, `HairStrands`, `Chooser`, `Mover`, `NetworkPrediction`, `ChaosMover`,
`AnimationLayering`, `MoverExamples`, `MovieSceneAnimMixer`, `DrawDebugLibrary`,
`SmartObjects`, `Locomotor`, `CurveExpression`, `GameplayInteractions`.

Demonstrably used by the CMC character, from its component list: **MotionWarping**
(`MotionWarpingComponent`), **SmartObjects** (`AC_SmartObjectAnimation`), **Chooser** +
**PoseSearch** (traversal + motion matching), GameplayCameras (`GameplayCameraComponent`).
**Mover / ChaosMover / MoverExamples / NetworkPrediction** are used only by the Mover
character — dead weight the moment the project commits to CMC.

---

# Part B — Modifiability

## B1 · Split high obstacles into a custom state, low ones untouched — **conditional, yes**

Two routes, and the cheap one is not the obvious one.

**Route 1 — free, no code.** GASP traversal only fires on `LevelBlock_Traversable`. Author
low obstacles as that class; leave high ledges as plain static meshes. GASP handles the
first and is *structurally incapable* of touching the second. Cost: zero. Weakness: the
split is by authoring convention, not by a per-character number — Samer and Youssef cannot
differ, which the handoff requires.

**Route 2 — the per-character threshold.** `TryTraversalAction` is called from the
character's **EventGraph**, and event graphs are not overridable in a child Blueprint, so
the call cannot be intercepted without editing a GASP asset. But `GetTraversalCheckInputs`
is a **function** on `SandboxCharacter_CMC`, and functions *are* overridable. Override it in
our child: run our own forward trace first, and if the obstacle is above the data-asset
threshold, return inputs that make the check fail, then enter our state. Below the
threshold, call `Super` and let GASP run untouched.

**No GASP asset is edited in either route.** Recommend Route 2, with Route 1 as the fallback
if overriding proves to have side effects.

## B2 · Reuse the catch, discard the automatic climb-up — **yes, and the cut point is exact**

Notifies on `AM_M_Neutral_Traversal_Catch_Cliff_high_standF`:

| Notify | Start | Duration | Ends |
|---|---|---|---|
| `AnimNotifyState_MotionWarping` (target `FrontLedge`, `SkewWarp`) | **0.905** | 0.140 | **1.046** |
| `AnimNotifyState_PoseSearchBranchIn` | 0.905 | 0.035 | 0.940 |
| `BP_NotifyState_TraversalBlendOut_C` | **2.499** | 6.801 | past the 5.0 s end |

**Cut at ≈1.05 s** — the instant the warp window closes and the hands are on the ledge.
1.05 → 2.50 is the automatic climb. From 2.499 the montage is already blending back to
locomotion.

`PoseSearchBranchIn` closing at 0.940, just before the cut, means motion matching is
*already* primed to take over at exactly that point. Epic put the branch there.

Cheapest method: call `Montage_Stop` at 1.05 s from our state — **no asset touched at all.**
Alternative: duplicate the montage into `/Game/Parkour/` and trim (permitted; still not an
in-place edit). Prefer the first.

## B3 · Custom mode coexisting with the AnimBP without editing it — **conditional**

Chooser columns bind by **property binding** to indexed context objects, not by tag. A
gameplay tag reaches a chooser only through a column bound to a property we can write.

We cannot add a column to `CHT_TraversalMontages_CMC` without editing a GASP asset. We do
not need to: we author **our own** chooser table in `/Game/Parkour/`, bound to our own
context struct, and evaluate it from our own C++. GASP's table stays untouched.

The AnimBP itself needs no edit for a montage-driven state, because the montage plays in
`DefaultSlot`, which the graph already services. Step 4.1 proves GASP already pushes
gameplay data into the AnimBP through an interface — that same interface pattern is the
supported channel if we later need state to reach the graph.

**Unverified:** whether our new state can reach the pose-search chooser *without* an AnimBP
edit. That depends on the graph's existing inputs, which I did not read.

## B4 · Trajectory / prediction in a zero-velocity custom mode

The concrete failure: motion matching and the trajectory generator read CMC `Velocity` and
`Acceleration`. In a custom mode that moves the capsule by root motion or by direct
`SafeMoveUpdatedComponent`, `Velocity` can be zero while the character visibly travels. The
predicted trajectory collapses toward a point, the pose search reads that as "standing
still", and it selects idle — while the body is moving. Symptom: the character slides in an
idle pose, worst at the moment of entry and exit.

Two mitigations: freeze/exclude prediction inside the custom mode, or write a synthetic
`Velocity` each tick matching the visual motion. The second keeps the graph honest and is
what a long hang needs.

**I did not identify the exact trajectory component on this character by name.** See
confidence table — this is the single most important unverified item in Part B.

## B5 · Anything assuming locomotion is always active

One concrete item, read: `BP_NotifyState_TraversalBlendOut_C` on the catch montage has a
duration of **6.801 s** on a **5.0 s** montage — it deliberately outlives the montage. If we
stop the montage early at 1.05 s we must confirm its `NotifyEnd` still fires, or the
blend-out state leaks.

The `..._FromTraversal` pose search databases exist specifically to catch the exit from a
traversal. A hang that ends without passing through the normal exit may find no matching
pose.

Beyond that I did not audit the AnimBP graph, so I cannot claim this list is complete. A
long stationary hang is exactly the case GASP has never been asked to handle.

## B6 · Per-state cost — **and how to shorten it**

As the code stands, adding one new movement state touches **seven** places:

1. Movement component — new `CustomMovementMode` enum value and a `PhysCustom` branch
2. A new C++ state class
3. A chooser table row or a new table
4. A pose search database entry or a montage binding
5. A gameplay tag in `DefaultGameplayTags.ini`
6. An input action plus an `IMC_Sandbox` mapping
7. A per-character data asset entry

Seven is too many, and the handoff asks for a proposal when it exceeds five.

### Proposal — one generic mode, states as data

Collapse 1, 3, 5 and 7 into the data asset:

```
CMOVE_Scripted                  ← ONE custom movement mode, forever
  └─ delegates every tick to the active ULTHTraversalState*

ULTHTraversalState (UObject, Instanced, Blueprintable)
  virtual Enter / Tick / Exit / CanEnter
  UPROPERTY: GameplayTag, Montage (TSoftObjectPtr), ChooserTable, per-state numbers

ULTHTraversalStateSet : UPrimaryDataAsset
  TArray<TObjectPtr<ULTHTraversalState>>   ← per character; asymmetry is this array
```

A new state then costs **two things**: one C++ class deriving from `ULTHTraversalState`, and
one entry in the character's state-set data asset. The tag, the montage, the chooser table
and the numbers are all fields on the state object — authored, not coded. Input stays
generic: the existing parkour-up / parkour-down actions ask the state set *which state can
be entered now*, so a new state needs no new button.

This is the deliverable that makes "many bespoke mechanics, one per chapter" affordable. It
should be built with the **first** layer, not retrofitted after the third.

---

# Part C — Animation inventory and gaps

## C1 · What exists

Project totals from the asset index: **2008 `AnimSequence`, 137 `AnimMontage`, 20 `AnimBlueprint`, 128 `PoseAsset`**.

Traversal content lives under `/Game/Characters/UEFN_Mannequin/Animations/Traversal/`:

| Folder | Content |
|---|---|
| `Catch/Vault/` | `M_{Neutral,Relaxed}_Traversal_Catch_Vault_{low,med,high}` + `AM_` montages |
| `Catch/Hurdle/` | `..._Hurdle_{low,med,high}_{stand,walk,run}` × Neutral/Relaxed |
| `Catch/Mantle/` | `..._Mantle_{low,med,high}_{stand,walk,run}` × Neutral/Relaxed |
| `Catch/Cliff/` | **`AM_M_Neutral_Traversal_Catch_Cliff_high_standF`**, `AM_M_Relaxed_Traversal_Catch_Cliff_high_stand`, `M_Neutral_Traversal_Catch_Cliff_high_stand` |
| `Climb/` | `AM_M_{Neutral,Relaxed}_Traversal_Climb_Start_2_5_{stand,walk,run}_F_{Lfoot,Rfoot}` |

Root motion, duration and notifies were read for the cliff catch only (§B2). The rest were
not opened individually — see confidence table.

## C2 · Hanging content — **a complete set exists. My first answer was wrong.**

> **Correction.** This section originally read "none", rated High confidence, justified as
> "two independent project-wide searches". Both searches were mine and both were bad. The
> first searched for `Hang`; these clips are named `Ledge`. The second passed
> `asset_class: "AnimSequence,AnimMontage"` as one string, which the filter received as the
> single literal `["AnimSequence,AnimMontage"]` and matched nothing. I read an empty result
> from a malformed query as a fact about the project, and the word "independent" was doing
> work it had not earned. The owner caught it.

`/Game/_Project/animation/Ledge Animation Set/` holds **97 assets — 39 clips authored twice,
once In-Place and once Root Motion**, plus its own mannequin and an overview map.

| Group | Clips |
|---|---|
| Catch | `Ledge_Grab`, `Ledge_Grab_L`, `Ledge_Grab_R` |
| Hold | **`Ledge_Idle`** |
| Shimmy — fast | `Ledge_Run_L` / `_R`, each with `_Start` and `_Stop` |
| Shimmy — hand shuffle | `Ledge_Lean_L` / `_R` / `_Up` / `_Down` / `_Out`, each with `_Start` and `_Stop` |
| Corners | `Ledge_Corner_Inside_L` / `_R`, `Ledge_Corner_Outside_L` / `_R` |
| Climb | `Ledge_Climb_Up`, `Ledge_Climb_Down` |
| Ledge-to-ledge | `Ledge_Jump_Up` / `_L` / `_R` / `_Down` / `_Out` |
| Exit | `Ledge_Drop`, `Falling` |

`RM_` variants carry `bEnableRootMotion`. The set is *start / loop / stop* per direction,
which is the shape a motion-matched or state-driven shimmy wants — not a bare loop.

The owner's description maps exactly: **`Run_*` is the fast traverse, `Lean_*` is the
hand-over-hand shuffle.** Two speeds of the same verb, which is a design input, not a
duplicate.

**Two other packs sit beside it:** `Zipline Animation Pack` (35 assets) — direct L4 material —
and `114 Roll and Dodge Mocap Animations` (387 assets) — landing, roll and dodge. Ten sibling
folders (`Climbing`, `wall`, `vault`, `mantle`, `Sliding`, `Parkour`, `Predictive`, `base`,
`drop`, `landing`) exist and are **empty**.

### The real constraint: wrong skeleton

```
RM_Ledge_Idle  →  /Game/LedgeAnimationSet/Characters/Mannequin/Mesh/UE4_Mannequin_Skeleton
GASP           →  /Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin
```

**Every clip in the set is on `UE4_Mannequin_Skeleton` and must be retargeted before it can
play on this character.** The internal reference also still points at `/Game/LedgeAnimationSet/`
rather than where the pack now lives, so it is resolving through a redirector — the project
carries 86 `ObjectRedirector` assets.

The path is already in the project, both halves of it:

| Retargeter | Direction |
|---|---|
| `/Game/_Project/animation/114 Roll and Dodge Mocap Animations/DEMO/Characters/Mannequin_UE4/Rigs/RTG_UE4Manny_UE5Manny` | **UE4 → UE5** |
| `/Game/Characters/UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin` | UEFN ↔ UE5 |

Monolith exposes `animation.batch_retarget_animations`, so this is one scripted pass over 78
clips, not 78 manual retargets.

## C3 · Missing clips

**L1 Ledges — 11 missing**

| Clip | Required? |
|---|---|
| `AM_Hang_Idle` — braced hang loop, in place | **required** |
| `AM_Hang_ClimbUp` — hang → standing on top | **required** |
| `AM_Hang_Drop` — deliberate release, push off | **required** |
| `AM_Hang_Shimmy_L` / `_R` — loops, fixed step length | **required** |
| `AM_Hang_Corner_Inner_L` / `_R` | polish |
| `AM_Hang_Corner_Outer_L` / `_R` | polish |
| `AM_Hang_Jump_Up` / `_L` / `_R` | polish |

**L2 Wall run — 5 missing:** entry, horizontal loop, vertical loop, eject, wall-to-hang
transition. All required for a playable version; there is no wall-run content of any kind.

**L3 Beam / balance — 6 missing:** mount, walk loop, turn-in-place, stop, dismount,
fall-off. Only the fall-off is derivable.

**L4 Rope / bar / swing — 8 missing:** tightrope loop, monkey-bar reach L/R, bar hang idle,
bar swing, rope grab, rope swing loop, release. All required.

## C4 · What can be derived

| Missing | Derivable? |
|---|---|
| `AM_Hang_Idle` | **Partly.** The cliff catch's pose at ~1.0 s *is* a hang pose. A single-frame pose extracted there, plus a breathing additive, is a serviceable placeholder loop. |
| `AM_Hang_ClimbUp` | **Partly.** `Traversal_Climb_Start_2_5` is a ground-start climb; its upper half is close. Retiming and cutting its entry may serve as a grey-box. |
| `AM_Hang_Shimmy_R` | **Yes, from `_L`** — mirrored. A mirror data table is needed; the pose search schema supports one. |
| Corners, ledge jumps | No. Must be authored. |
| Beam walk loop | **Yes, partly.** An existing walk loop plus an arms-out additive lean, through the `AnimationLayering` plugin which is already enabled. Not good, but visibly a balance walk. |
| Beam fall-off | Yes — existing fall/land content. |
| Wall run, rope, bars | No. Nothing in GASP resembles them. |

## C5 · Reference offsets — **and a correction**

**GASP traversal clips have no fixed reference offset.** Epic's own comment inside
`SetWarpTargets` says so: *"the traversal animations move at different distances (no fixed
metrics)"*. GASP solves it by baking a curve named **`Distance_From_Ledge`** into each clip
and sampling it at the end of each warp window with `GetCurveValueFromAnimation`.

This corrects `ANIMATION_SPEC.md`, which tells the owner to author every hanging clip at one
constant hand-to-wall distance. That rule is not how this project works. The GASP-native
requirement is stricter and more flexible at once:

> **Every authored clip must carry a `Distance_From_Ledge` float curve**, measuring the
> distance from the front ledge over the clip's length.

A constant offset is still worth keeping as a *convention* — Epic themselves say fixed
metrics "would be an improvement" — but the curve is what the system actually reads.

Measured offsets I can state: the `FrontLedge` warp window on the cliff catch runs
**0.905 → 1.046 s**, so any replacement catch must reach its ledge contact inside a window of
that shape. Hand-to-wall distance in world units was **not** measured; it requires reading
bone transforms at a specific frame — possible with Monolith
(`animation.get_animated_bone_transform`) but not done.

---

# Part D — Build / Buy / Derive

| Layer | Build | Derive | Buy |
|---|---|---|---|
| **L1 Ledges** | Detection, hang state, shimmy stepping, corner logic: **4–6 days** on top of what exists | Hang idle from the catch pose; shimmy R mirrored from L | — |
| **L2 Wall run** | Surface-normal projection, contact loss, eject: **3–4 days** | Nothing | — |
| **L3 Beam** | Spline attach, lateral offset, fall-off: **2–3 days** | Beam walk as walk + additive lean | — |
| **L4 Rope / swing** | Spline + pendulum integrator: **4–5 days** | Nothing | — |

## Buy — the honest answer

**I can verify exactly one purchasable asset: the Dynamic Parkour Trace System the owner
already owns.** It is a *project* (`DPTS_Assets.uproject`), not a plugin; it has **zero**
C++, a 9.8 MB principal Blueprint, 79 configuration structs, and **zero parkour animations**
(its 45 animation files are demo content on the standard mannequin).

I will not name any other marketplace product. I cannot inspect their contents, their
licences, or whether they ship animation, and naming one on reputation would be a guess
dressed as advice — which this study exists to avoid.

**Recommendation: no purchase.** Nothing buyable supplies the thing this project actually
lacks, which is eleven hanging clips on the UEFN Mannequin skeleton carrying a
`Distance_From_Ledge` curve. Money is better spent on authoring those. DPTS keeps one real
use: its 79 config structs are a **specification** of the action list, readable now through
Monolith, from which the decision layer's *shape* can be ported into C++ — which is
precisely what `PARKOUR_HANDOFF.md` §9 permits.

## L3 + L4 as one system — **assumption confirmed, with one caveat**

Both are "a transform constrained to a curve, with an input-driven parameter along it". The
difference is one term: L4 adds a pendulum integrator and a release impulse; L3 has neither.
A single `ULTHSplineAttachState` with an optional pendulum term covers both.

**The caveat:** monkey bars are *not* one of these. Discrete rungs are a sequence of
point-to-point reaches, closer to L1's ledge-to-ledge jump than to a spline walk. Budget
monkey bars with L1, not L4.

---

# Part E — Build order

> **Revised after the C2 correction.** The argument below rested on "L3 is the only layer
> with derivable animation". That is no longer true: **L1 now has a complete 39-clip set and
> L4 has a zipline pack.** L1 has gone from the most animation-starved layer to the best
> supplied one in the project, and it already has working detection code.
>
> **I now recommend L1 first**, on the same reasoning that originally favoured L3 — build the
> layer whose animation exists. The condition below is unchanged and is the part that matters:
> the first deliverable is the state template, not the layer. L3 moves to second, where it
> still proves the spline path for L4 but costs less because the template already exists.
>
> The original argument is kept below rather than deleted, because it was right given what I
> then believed, and the thing that changed was a fact, not a judgement.

## ~~L3 first~~ — the original argument, superseded

The owner's reasoning holds, and two things I read strengthen it:

1. **The spline pattern is already proven in this project.** `LevelBlock_Traversable` carries
   four `SplineComponent`s and `GetLedgeTransforms` queries them. L3 is not new
   infrastructure; it is the same query against a different curve.
2. **L3 is the only layer with a derivable animation.** Walk loop + additive lean through
   the already-enabled `AnimationLayering` plugin gives a visible balance walk for free. L1,
   L2 and L4 all stall on clips that do not exist. Starting with the layer that needs no
   purchase is the correct order when the animation budget is the constraint.

**The condition:** the first deliverable is the **state template of B6**, not the beam. If L3
ships as a bespoke beam system, the fourth layer costs the same as the first and the study
was wasted. The beam is the proving case; the template is the product.

**What this costs:** the existing `ULTHLedgeHangMode` / `ULTHLedgeClimbUpMode` work is not
discarded. Its detection is geometry and ports unchanged; its two Mover modes are rewritten
as `ULTHTraversalState` subclasses. That rewrite has to happen regardless, because the
project is moving to CMC.

**Revised order:** L3 (template + beam) → **hard gate** → L1 (port existing detection into
the template) → L2 → L4.

## First layer — concrete file list

No implementation; names and ownership only.

```
Source/lastTwoHours/Movement/
  LTHCharacter.h/.cpp               ACharacter subclass. Exists only to call
                                    SetDefaultSubobjectClass<ULTHMovementComponent>
                                    in its constructor. Blocker §2 forces this.

  LTHMovementComponent.h/.cpp       : UCharacterMovementComponent
                                    Owns CMOVE_Scripted and its PhysCustom branch.
                                    Delegates every tick to ActiveState. Owns nothing else.
                                    Talks to: nothing above it.

  LTHTraversalState.h/.cpp          : UObject, Instanced, Blueprintable. The template.
                                    CanEnter / Enter / Tick / Exit. Holds GameplayTag,
                                    TSoftObjectPtr<UAnimMontage>, per-state numbers.
                                    Talks to: LTHMovementComponent, MotionWarpingComponent.

  LTHTraversalStateSet.h/.cpp       : UPrimaryDataAsset. TArray of instanced states.
                                    One asset per character — this is where Samer and
                                    Youssef diverge. IsDataValid() complains about a
                                    state with no montage bound.

  States/LTHSplineAttachState.h/.cpp  : ULTHTraversalState. Beam and, later, rope.
                                    Optional pendulum term, off for the beam.
                                    Talks to: USplineComponent on the beam actor.

Source/lastTwoHours/World/
  LTHTraversableSpline.h/.cpp       AActor with a USplineComponent and a
                                    FGameplayTag describing what it affords
                                    (Beam / Rope / Tightrope). Deliberately NOT a
                                    child of LevelBlock_Traversable — that class is
                                    GASP's and its four ledge splines mean
                                    something else.

Source/lastTwoHours/Debug/
  LTHTraversalDebug.h/.cpp          p.Parkour.Debug, p.Parkour.DebugPlayer <index>,
                                    p.Parkour.FreezeDecision. Per-player on-screen
                                    line. Every rejection returns an explicit reason.
                                    Handoff §8 — part of the deliverable, not a follow-up.
```

Blueprint work for the owner, in one batch: re-parent the character chain to `LTHCharacter`,
author one `LTHTraversalStateSet` per character, place one `LTHTraversableSpline` in the
ruler map, bind the input action.

---

# Blockers

**1 · The project is not on CMC. Nothing has moved yet.**
`GM_Phase0.DefaultPawnClass` is `BP_L2H_Pawn_Phase1`, whose parent is
`SandboxCharacter_Mover_C`. Every locked decision in `PARKOUR_HANDOFF.md` assumes CMC. The
existing `ULTHLedgeHangMode` / `ULTHLedgeClimbUpMode` are **Mover** movement modes and do
not survive the switch — they are rewritten, not ported. The detection geometry does survive.

**2 · A custom movement mode on CMC requires a C++ character class.**
`CharMoveComp` is a **native subobject of `ACharacter`**. Its class cannot be changed from a
Blueprint. `UParkourMovementComponent` therefore requires an `ACharacter` subclass in C++
calling `SetDefaultSubobjectClass`, and `SandboxCharacter_CMC`'s child chain must be
re-parented onto it. The handoff's instruction to create the component "now, even if empty"
is correct — this is the cost it was avoiding. It cannot be deferred without paying it twice.

**3 · GASP traversal is inert on ordinary geometry.**
It requires `LevelBlock_Traversable` actors with four authored ledge splines. Any plan that
says "GASP handles low obstacles automatically" is wrong as written — GASP handles low
obstacles *on authored blocks*. For a game of bespoke per-chapter geometry this is a real
authoring cost that must be budgeted or designed around.

**4 · The ledge clips are on the wrong skeleton.**
All 78 are `UE4_Mannequin_Skeleton`; GASP is `SK_UEFN_Mannequin`. Nothing plays until they are
retargeted. Both retargeters needed already exist in the project (§C2), and Monolith can batch
it — so this is a half-day of scripted work, not a purchase. It is a blocker only in the sense
that **no ledge work can be tested until it is done**, which makes it the first thing to do.

**5 · `ANIMATION_SPEC.md` gives the wrong authoring rule.**
It requires a constant hand-to-wall offset. GASP has no fixed metrics and reads a
per-clip `Distance_From_Ledge` curve instead. Clips authored to the current spec will warp
incorrectly. That file must be corrected before any clip is commissioned. **This is the
cheapest blocker to fix and the most expensive to discover late.**

**5 · One input mapping context for two players.**
`IMC_Sandbox` is the only IMC in the project. Workable, but there is no per-player context
today, so per-character input divergence has nowhere to live yet.

---

# Decisions taken without instruction

| Decision | Rejected alternative |
|---|---|
| Recommend Route 2 (override `GetTraversalCheckInputs`) for the high/low split | Route 1, authoring-by-convention — free, but cannot express a per-character threshold |
| Stop the catch montage at 1.05 s rather than duplicating and trimming it | A trimmed duplicate in `/Game/Parkour/`; legal, but an asset to maintain for a result one function call achieves |
| Recommend **no purchase** | Naming marketplace parkour assets on reputation — I cannot verify their contents or licences, and this study exists to stop exactly that |
| Agree with L3-first, but make the state template the deliverable | Disagreeing and starting with L1 because it is half-built. Rejected: L1's blocker is animation, and the template is needed either way |
| Budget monkey bars with L1, not L4 | Filing them under "rope and bar" as the brief implies — they are discrete reaches, not a spline walk |
| `LTHTraversableSpline` as its own actor, not a `LevelBlock_Traversable` child | Reusing GASP's class — its four ledge splines carry a different meaning and the cast in step 2.1 would then catch our beams |

---

# Confidence table

| Claim | Confidence | What would settle it |
|---|---|---|
| Capsule 86/30, `MaxStepHeight` 45, all A1 numbers | **High** | Read from the CDO directly |
| Traversal requires `LevelBlock_Traversable` | **High** | The `Cast To` node is in `TryTraversalAction`; a failed cast exits |
| Chooser columns and the four rows | **High** | Read column by column |
| ~~Zero hanging animation~~ — **WRONG, see C2** | was High, actually false | A complete 39-clip ledge set exists. The searches that "proved" zero used the wrong term and a malformed class filter. **Lesson recorded: an empty result from a query I wrote is evidence about my query first, and about the project second.** |
| Ledge set is on `UE4_Mannequin_Skeleton` | **High** | Read the skeleton path out of `RM_Ledge_Idle.uasset` directly |
| Retarget UE4 → UEFN will preserve the hang poses | **Medium** | Both retargeters exist, but a hang is an extreme arms-overhead pose and retargeting those is where wrist and shoulder error shows. Retarget `RM_Ledge_Idle` first and look at it |
| Catch cut point ≈1.05 s | **High** | Notify start + duration read from the montage |
| `Distance_From_Ledge` is the real offset mechanism | **High** | Epic's own comment plus the `GetCurveValueFromAnimation` nodes |
| Enum mapping 1=Hurdle, 2=Mantle, 3=Vault | **Medium** | Row 2 (no back floor) → 2 is consistent with Mantle, but the chooser reports raw `NewEnumeratorN` names. Open `E_TraversalActionType` |
| `GetTraversalCheckInputs` is overridable in a child | **Medium** | It is reported as a function graph, and BP functions are overridable — but not tested. Create a child and try to override it |
| **Which trajectory component breaks in a zero-velocity mode** | **Low** | Not identified by name. Inspect the AnimBP's trajectory/pose-history nodes and the character's trajectory component |
| `MovingTraversal` / `JustTraversed` usage | **Low** | Not read. `animation.get_nodes` on `SandboxCharacter_CMC_ABP` |
| Warping node order, Offset Root Bone state | **Low** | Not read. Same query |
| Per-gait speeds | **Low** | CDO shows 500; GASP sets gait speeds at runtime. Read the character's gait data |
| No player-0 assumption in Blueprints | **Low** | Only C++ was grepped. `blueprint.search_nodes` for `GetPlayerCharacter` across `/Game` |
| Exact `PoseSearchDatabase` count | **Low** | Query returned a full page of 60; paginate |
| Day estimates in Part D | **Low** | Estimates, not measurements. Only the first layer's actual duration will calibrate them |
| Hand-to-wall distance of existing clips | **Not measured** | `animation.get_animated_bone_transform` on the hand bone at t≈1.0 s of the cliff catch |

---

# First five things to test in the editor, ranked by risk

1. **Can a child Blueprint override `GetTraversalCheckInputs`?** The entire high/low split
   rests on it. Right-click → Override in a child of `SandboxCharacter_CMC`. If the function
   does not appear in the override list, Route 2 is dead and B1 falls back to Route 1.
2. **Re-parent one character to a C++ `ACharacter` subclass and confirm the movement
   component swaps.** Blocker §2. If `SetDefaultSubobjectClass` does not take on a chain this
   deep, every later phase is affected.
3. **Play the cliff catch and stop it at 1.05 s.** Confirm the pose is a usable hang and that
   `BP_NotifyState_TraversalBlendOut_C` does not leak when the montage ends early.
4. **Stand still in a zero-velocity custom mode and watch the lower body.** B4's failure is
   visible immediately: idle feet under a travelling body.
5. **Build one `LevelBlock_Traversable` and one plain static-mesh wall of identical size.**
   Confirms A3 by demonstration — traversal on the first, nothing on the second — and it is
   the cheapest possible addition to the ruler map.
