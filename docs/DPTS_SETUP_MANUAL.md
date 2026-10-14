# دليل تركيب DPTS على GASP — خطوة بخطوة

المشروع: `lastTwoHours` · المحرّك: UE 5.8 · النظام: Dynamic Parkour Trace System V3

**Blueprint فقط. لا C++ في هذا المسار إطلاقاً.**

---

## ما القرار المعماري، ولماذا

| الطبقة | من يتولّاها |
|---|---|
| الحركة الأساسية (مشي، ركض، قفز) | **GASP** — Motion Matching وكليباته الـ2,345 |
| مكوّن الحركة | **CMC** — `CharacterMovementComponent` |
| الباركور والحافة | **DPTS V3** |

**GASP وCMC ليسا بديلين.** GASP مشروع أنيميشن يشحن نسختين من الشخصية: واحدة على CMC وأخرى على Mover. اخترنا نسخة CMC. المتروك هو **Mover** وحده — وهو إضافة تجريبية كلّفت أسبوعين قبل أن تُترك.

**ولماذا DPTS بدل عبور GASP؟** لأن GASP **لا يملك تعلّقاً بالحافة**. إمساكاته الأربع (`Catch_Cliff`, `Catch_Mantle`, `Catch_Hurdle`, `Catch_Vault`) كلها تنتهي واقفاً فوق العائق. و`Catch_Cliff` تحديداً مونتاج طوله **5 ثوانٍ**: ثانية إمساك ثم أربع ثوانٍ تسلّق ووقوف.

---

## سلسلة الوراثة

```
ACharacter  (محرّك، C++)
   └── SandboxCharacter_CMC   (GASP — لا يُعدَّل أبداً)
         └── BP_L2H_CharacterBase   (شخصيتنا — كل تعديلاتنا هنا)
               ├── BP_SAMER
               └── BP_YOUSSEF
```

**قاعدة:** لا يُعدَّل أصل من Epic. كل شيء في الابن. سبق أن عُدِّل `AC_TraversalLogic` فصار أكبر بـ37 كيلوبايت من الأصل، وعطّل العبور كلّه حتى استُرجع من العيّنة.

---

## المرحلة ١ — التمهيد

### 1.1 إضافة Motion Warping

`Edit ▸ Plugins ▸` ابحث عن **Motion Warping** ▸ فعّل ▸ أعد تشغيل المحرّك.

> في هذا المشروع **مفعّلة أصلاً** — GASP يعتمد عليها.

### 1.2 الشخصية على CMC

`BP_L2H_CharacterBase ▸ Class Settings ▸ Parent Class` = **`SandboxCharacter_CMC`**

**علامة النجاح:** يظهر مكوّن **`Character Movement (CharMoveComp)`** في قائمة المكوّنات. لو رأيت `CharacterMover` بدلاً منه، فالشخصية ما زالت على Mover ولم يقع التبديل.

---

## المرحلة ٢ — تركيب DPTS

### 2.1 المكوّن

`Add ▸` ابحث عن **`Dynamic Parkour Trace System`**

الصنف الصحيح: **`DynamicParkourTraceSystem_C`** — بلا شُرَط سفلية.
`Dynamic_Parkour_Trace_System` (بشُرَط) هو **الإصدار القديم**؛ إن ظهر فالمشروع فيه تثبيتان.

### 2.2 الواجهة

`Class Settings ▸ Interfaces ▸ Add ▸` **`DPTS_BPI`**

تضيف 11 دالّة:

| الفئة | الدوالّ |
|---|---|
| **SetData** | `SetSystemUseTransform` · `SetTransitionUseValues` · `SetCustomRetimeValue` · `StateResetExecution` |
| **ActionEvents** | `Execute_LedgeCatchAction` (Climb? bool) · `Execute_ClimbJumpAction` · `Execute_ClimbStartAction` · `Execute_ClimbTransitionAction` · `Execute_DropOffAreaAction` · `Execute_VaultMantleAction` · `Execute_WallrunJump` (FreeJump? bool) |

### 2.3 نقطة أصل الفحص

في `My Blueprint ▸ Interfaces` افتح **`SetSystemUseTransform`**:

```
Function Entry ──▸ Return Node
                     Transform ◂── Get Actor Transform
```

هذه هي النقطة التي تبدأ منها كل مِجَسّات DPTS. بلا توصيلها تعود مصفوفة صفرية وتفشل كل الفحوص.

---

## المرحلة ٣ — حسم نزاع زرّ القفز

### المشكلة

`IA_Jump` مشغول. مسار GASP في الأب:

```
IA_Jump ▸ Started
   └─▸ Branch
         condition ◂── CanPerformJumpAction()
         then      ──▸ AC_TraversalLogic.TryTraversalAction
```

لو وصّلت DPTS على القفز دون معالجة، **يعمل النظامان على نفس الضغطة ويتنازعان**.

### الحلّ — بوّابة يكشفها GASP نفسه

`CanPerformJumpAction` دالّة **عامّة وقابلة للتجاوز**، وهي الشرط الوحيد لعبور GASP.

**3.1 — التجاوز**

`My Blueprint ▸ Functions ▸` مرّر على **`Override`** ▸ اختر **`Can Perform Jump Action`**

في الدالّة: صِل `Function Entry ▸ Return Node` واترك `ReturnValue` = **`False`**.

> بهذا يُغلق عبور GASP **بلا لمس أصل واحد من Epic**. ولو أردت لاحقاً خلط النظامين، تُرجِع شرطاً بدل `False` الثابتة.

**3.2 — توصيل القفز لـDPTS**

في `EventGraph`:

```
EnhancedInputAction IA_Jump ▸ Started
   └─▸ Branch
         Condition ◂── NOT Boolean ◂── Get Busy  (من مكوّن DPTS)
         True      ──▸ SearchForParkourAction
```

`Busy` يمنع بدء حركة جديدة أثناء تنفيذ واحدة.

---

## مفاصل GASP الثلاثة — وهي كلّها ما يلزم

الأب `SandboxCharacter_CMC` يكشف ثلاث دوالّ **عامّة قابلة للتجاوز**، وهي كل ما يقوده كل إطار.
تجاوزها في الابن هو طريق التمديد الذي صمّمه Epic — لا حاجة لمضاعفة الشخصية.

| الدالّة | ما يفعله الأب | البوّابة |
|---|---|---|
| `CanPerformJumpAction` | شرط عبور GASP على زرّ القفز | تُرجع `False` |
| `UpdateMovement_PreCMC` | **يعيد كتابة السرعة والتباطؤ كل إطار** | `Switch on S_ParkourStates` ▸ `None` فقط ▸ `Parent:` |
| `UpdateRotation_PreCMC` | يدير الشخصية كل إطار | نفس البوّابة |

بلا البوّابة الثانية، يُمسح `MaxSpeed = 150` و`BrakingDeceleration = 1200` اللذان يضبطهما DPTS
للتسلّق، فيتحرّك اللاعب بسرعة GASP الكاملة بلا تباطؤ — **وهذا يظهر كأن فحص المساحة معطّل،
وهو في الحقيقة يُمسح.**

### لماذا تبقى الشخصية طفلاً، ويُضاعَف الـABP

| | القرار | السبب |
|---|---|---|
| الشخصية | **طفل** | المفاصل الثلاثة موجودة، ومقارنة البصمة بـ`sample/` تبقى ممكنة — وبها كُشف عطب `AC_TraversalLogic` |
| `SandboxCharacter_CMC_ABP` | **يُضاعَف** | لا مفصل: صفر `AnimLayerInterface`، وصفر `linked_anim_blueprints` |

> المضاعفة ليست ممنوعة بل **غير لازمة للشخصية**. وأب `SandboxCharacter_CMC` هو `Character`
> الأصلي، والـABP لا يحوي `Cast` إلى الشخصية (يقرأ عبر `S_CharacterPropertiesForAnimation`
> المدفوعة إليه) — فالمضاعفة ممكنة تقنياً، لكنها تُميت فحص السلامة بلا مقابل.

## التحقّق — قبل أي تجربة

| ما يُفحص | القيمة الصحيحة |
|---|---|
| المكوّن | `DynamicParkourTraceSystem_C` |
| الواجهات | `DPTS_BPI_C` — غير موروثة |
| `CanPerformJumpAction` | موجودة في `Functions`، تُرجع `false` |
| `SetSystemUseTransform` | 3 عقد، متّصلة بـ`GetActorTransform` |
| `EventGraph` | `IA_Jump ▸ Branch ▸ SearchForParkourAction` |
| الترجمة | `UpToDate` · 0 خطأ · 0 تحذير |

---

## المرحلة ٤ — ربط الأنيميشن *(التالية)*

> طبقة الحافة والتسلّق لها أمر تنفيذ مستقلّ وموثّق بالأرقام:
> **[DPTS_LEDGE_BUILD.md](DPTS_LEDGE_BUILD.md)** — ابدأ منه، وأوّل خطوة فيه هي وسوم الأجسام.

تسعة جداول اختيار في `Content/DPTS_Assets/SelectionTables/`:

| الجدول | الحركة |
|---|---|
| `DPTS_Settings_SelectionTable` | الإعدادات — صفوف متعددة، تُبدَّل وقت التشغيل بـ`Settings Use Index` |
| **`DPTS_LedgeCatch_SelectionTable`** | **الإمساك بالحافة** |
| **`DPTS_ClimbJump_SelectionTable`** | **القفز بين الحواف** |
| `DPTS_ClimbStart_SelectionTable` | بدء التسلّق |
| `DPTS_ClimbTransition_SelectionTable` | الانتقالات والزوايا |
| `DPTS_DropOffAction_SelectionTable` | النزول |
| `DPTS_VaultMantle_SelectionTable` | القفز فوق العوائق والتسلّق |
| `DPTS_Transition_SelectionTable` | |
| `DPTS_WallrunJump_SelectionTable` | الجري على الجدار |

وقسم `LedgeCatchSettings` داخل جدول الإعدادات:

```
MaxForwardSearchDistance   MaxZSearchDistance
SearchStartingZOffset      SearchStartingForwardOffset
AreaLengthMaxSearch        IgnoreClimbActors?
```

الكليبات المتاحة: **38 كليب حافة** في `Content/_Project/animation/Ledge_Animation_Set/` على `SK_UEFN_Mannequin` — نفس هيكل الشخصية، فلا إعادة استهداف.

---

## الفحص الأول — قبل أي تشخيص

**لا تُصدّق أصلاً من Epic حتى تقارن بصمته بالعيّنة.** أصلٌ معدَّل في مكانه يكسر كل أبنائه،
ولا يظهر في أي سجلّ.

```bash
powershell -c "(Get-FileHash 'D:\epic\Epic Games\lastTwoHours\Content\Blueprints\SandboxCharacter_CMC.uasset').Hash -eq (Get-FileHash 'D:\epic\Epic Games\sample\GameAnimationSample 5.8\Content\Blueprints\SandboxCharacter_CMC.uasset').Hash"
```

`True` = سليم. `False` = **توقّف وأصلحه قبل أي شيء آخر**.

وثلاثة أرقام تحسم القصّة كاملة:

| المقارنة | ما تعنيه لو اختلفت |
|---|---|
| نسخة العمل ↔ العيّنة | الأصل معدَّل الآن |
| `HEAD` ↔ العيّنة | العطب مُلتزَم في التاريخ |
| نسخة العمل ↔ `HEAD` | العطب غير مُلتزَم — يُستعاد بـ`git checkout` |

**الاستعادة تتطلّب إغلاق المحرّك أولاً**، وإلا كتب من ذاكرته فوق ما استعدته.

```bash
git -C "D:/epic/Epic Games/lastTwoHours" checkout -- Content/Blueprints/SandboxCharacter_CMC.uasset
```

### سجلّ الحوادث

| التاريخ | الأصل | الفارق | الأثر |
|---|---|---|---|
| 2026-09-17 | `AC_TraversalLogic` | +37 كيلوبايت | عطّل العبور كلّه |
| 2026-09-17 | `DefaultLevel.umap` | بايت واحد | فرض وضع لعبة يشغّل شخصية أخرى |
| 2026-09-20 | `SandboxCharacter_CMC` | **+5,333 بايت** | معدَّل وغير مُلتزَم — `HEAD` والعيّنة متطابقان |

## أعطال معروفة وعلاجها

| العرض | السبب | العلاج |
|---|---|---|
| `Accessed None ... AC_TraversalLogic` | حُذف مكوّن موروث أو عُطِّل | لا تحذف المكوّنات الموروثة — أغلق البوّابة بـ`CanPerformJumpAction` |
| `Invalid movement mode on None` | صفّ فارغ في `MovementModes` | املأ المفتاح والقيمة أو احذف الصفّ |
| سيل `LogEOSSDK` و`LogHttp` | تليمتري Epic يفشل في الاتصال | `[Core.Log]` ▸ `LogEOSSDK=Error` و`LogHttp=Error` في `DefaultEngine.ini` |
| مكوّن بصنف مفقود | حُذف صنفه من C++ | احذف المكوّن من الشخصية |
| **الحفظ ينجح والملفّ لا يتغيّر** | **Aura تشغّل `AuraSandbox` فتحوّل كل كتابة إلى `Intermediate/Sandboxes/`** | **أطفئ صندوق Aura قبل أي عمل على الحركة** |

### صندوق Aura — أخطر عطل في المشروع

Aura تُدرج `FileSandbox` و`SandboxedEditing` في إضافاتها الاختيارية، وتنادي `EnableSandbox` لحظة
بدء عملها. بعدها يقرأ المحرّك من `Content/` ويكتب في:

```
Intermediate/Sandboxes/AuraSandbox/Sandbox/Game/...
```

`save_asset` يعيد `saved: true`، و`LogSavePackage` يسجّل نقلاً ناجحاً، والتحقّق يجري — **والملفّ
الحقيقي لا يتحرّك، بلا تحذير واحد.**

**الصندوق يشمل المشروع كلّه، لا مستوى ولا مجلّداً ولا شخصية.** لذا فكرة عزل Aura بمستوى منفصل
وشخصية منفصلة **لا تحمي عمل الحركة إطلاقاً**.

**قبل أي جلسة حركة:** تأكّد أن `Intermediate/Sandboxes/AuraSandbox/Sandbox/` خالٍ أو غير موجود.
وإن وُجد فيه ملفّ، فهو حفظ ضائع — يُستعاد بنسخه إلى مسار `Content` المقابل، لأن الصندوق يحفظ
بنية `Game\...` صحيحة.

---

## مبدأ عمل تعلّمناه بثمن باهظ

> **لا تبنِ على أصل قبل أن تفتحه وتقرأ أرقامه.**

أمثلة كلّفت المشروع أسبوعين:

- `Catch_Cliff` ظُنّ تعلّقاً من اسمه. مدّته **5 ثوانٍ** وينتهي واقفاً.
- `AC_TraversalLogic` كان معدَّلاً وأكبر بـ37 كيلوبايت. عطّل العبور كلّه.
- `DefaultLevel` كان يفرض وضع لعبة يشغّل شخصية أخرى. الفارق **بايت واحد**.
- تحوّل CMC بدّل جدول القرار من `CHT_TraversalMontages_Mover` إلى `_CMC` — والصفّ المضاف بقي في الجدول المهجور.

**اقرأ الرقم. لا تستنتجه من الاسم.**
