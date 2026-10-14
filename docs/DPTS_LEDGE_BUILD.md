# بناء طبقة الحافة على DPTS — أمر تنفيذ موثّق

المشروع: `lastTwoHours` · UE 5.8 · **Blueprint فقط**

كل رقم هنا **مقروء من المشروع**، ومصدره مذكور بجانبه. ما لم أستطع قراءته مكتوب صراحةً أنه غير مؤكَّد.

---

## ⛔ حاجز قبل كل شيء — صندوق Aura

```
[02.45.27] LogAura: EnableSandbox: loaded 'AuraSandbox' — active.
```

ما دام هذا الصندوق نشطاً، **كل حفظ يذهب إلى `Intermediate/Sandboxes/AuraSandbox/` لا إلى `Content/`**،
والمحرّك يقول «حُفظ» بلا كذب لأنه لا يعلم. أطفئه وأعد تشغيل المحرّك قبل الخطوة الأولى.

**فحص:** `Intermediate\Sandboxes\AuraSandbox\Sandbox\` يجب أن يكون خالياً أو غير موجود.

التفصيل في [DPTS_SETUP_MANUAL.md](DPTS_SETUP_MANUAL.md).

---

## أين الحواف الموسومة — وكيف تجدها دائماً

| الاسم في الـOutliner | الامتداد | قمّة الحافة | الوسوم |
|---|---|---|---|
| **`LEDGE_01`** | x 2400 → 3700 · y −100…100 | **Z = 350** | `climb` · `ledge` |
| **`LEDGE_02`** | x 4000 → 5700 · y −100…100 | **Z = 350** | `climb` · `ledge` |

ثلاث طرق لإيجادها:

1. **في المنظر:** نصّ أخضر ثلاثي الأبعاد مكتوب عليه `LEDGE 01 | climb + ledge` بحجم 80.
2. **في الـWorld Outliner:** اكتب `LEDGE` في خانة البحث — يظهران وحدهما.
3. **في اللعب:** `DebugSettings ▸ ShowTargets? = True` فتُرسم كرة عند الهدف المكتشف.

> اللون والنصّ على `TextRenderComponent` المدمج في `LevelBlock_C` — بلا أجسام مضافة،
> وبلا تصادم، فلا أثر على اللعب.

**عند وسم حافة جديدة:** أضف `climb` ثم نوعها، وسمِّ الجسم `LEDGE_NN` واكتب النصّ نفسه
حتى تبقى القاعدة واحدة.

---

## الخطوة صفر — الوسوم. **بدونها لا شيء يعمل إطلاقاً**

فحصتُ `Content/Levels/DefaultLevel.umap` — المستوى أحادي الملفّ، لا `__ExternalActors__`:

```
climb : 0    point : 0    beam : 0    wallrunning : 0
```

**لا جسم واحد في المستوى معرَّف كحافة.** طبقة التسلّق في DPTS كلّها مشروطة بوسمين في
`Details ▸ Actor ▸ Tags`:

| الفهرس | القيمة | المعنى |
|---|---|---|
| `0` | `climb` | الجسم يدعم الباركور |
| `1` | `ledge` · `point` · `beam` · `wallrunning` | نوع التسلّق |

> عند وسم عدّة أجسام دفعةً واحدة، **لا تُدخل فيها جسماً موسوماً مسبقاً** وإلا ظهر `Multiple Values`
> ومُنع التعديل الجماعي.

الأجسام الحاملة لـ`climb` تُستثنى تلقائياً من فحص تصادم جذع الشخصية، فلا تعيق الحركة أثناء التعلّق.

**هذه الخطوة وحدها تفسّر كل فشل في الحافة حتى اليوم.** ابدأ بجدار واحد فقط.

---

## الخطوة ١ — التقاط الحافة أثناء السقوط (`LedgeCatch`)

### ١.١ الإعدادات — **جاهزة، لا تلمسها**

مقروءة من `DPTS_Settings_SelectionTable ▸ NewRow ▸ LedgeCatchSettings`:

| الحقل | القيمة | مطابقة الدرس |
|---|---|---|
| `MaxForwardSearchDistance` | **90** | ✔ «شبكة 90×90» |
| `MaxZSearchDistance` | **90** | ✔ |
| `SearchStartingZOffset` | **60** | ✔ «إزاحة رأسية تبدأ من 60» |
| `SearchStartingForwardOffset` | **0** | — |
| `AreaLengthMaxSearch` | **150** | — |
| `IgnoreClimbActors?` | **False** | ✔ «مغلق ⇒ التصاق فوري بالحافة» |

DPTS يشحنها مضبوطة. لم نستعملها ولا مرّة.

### ١.٢ التفعيل

`DynamicParkourTraceSystem ▸ AllowedActionListing ▸ LedgeCatch = True`

### ١.٣ التوصيل — **عملك في المحرّك** (Monolith لا يستطيع إنشاء عقد أحداث الواجهات)

```
Event Walking Off Ledge ──▸ Delay (0.3) ──▸ Execute In Air Searches
```

و`Event Execute_LedgeCatchAction` (من `DPTS_BPI`) يُوصَل إلى `AnimSelection_LedgeCatch`.

> `Walking Off Ledge` حدث `Character` أصيل — متاح لأن الشخصية على CMC.
> التأخير 0.3 ثانية ينظّم البحث ويمنع الالتقاط اللحظي عند بداية السقوط.

### ١.٤ أولوية الاختيار

`AnimSelection_LedgeCatch ▸ Priority` = **`StateTypeMatch`** (`E_SelectionPriorityClimb = NewEnumerator2`)

لأن نوع الحافة يجب أن يُفحص قبل الأرقام.

---

## الخطوة ٢ — أول مونتاج: `AM_L2H_Ledge_Grab`

المصدر: `Content/_Project/animation/Ledge_Animation_Set/SourceFiles/RM_Ledge_Grab`
— على `SK_UEFN_Mannequin`، **لا إعادة استهداف**.

الوصفة المثبَتة (نجحت في القفز فوق العوائق):

1. ضاعف الكليب إلى `Content/_Project/Parkour/Anims/`
2. `force_root_lock = false` ← **على المونتاج، لأن حركة الجذر هي ما ينقل الشخصية**
3. أنشئ المونتاج بـ`AnimMontageFactory` + `source_animation` (لا `create_montage`، فهو يعطي `duration: 0`)
4. `bEnableAutoBlendOut = false`

### الإشعارات

| # | الإشعار | القيمة |
|---|---|---|
| 1 | `AN_MotionWarp_WithOffset` | `WarpTargetName = height` — **أكّده بصرياً قبل أن تبني الثاني** |
| 2 | `AN_ResetParkourAction` | `BlendTime = 0.2`، و`Activate State = True` للدخول في حالة التسلّق |

> **لماذا «أكّده بصرياً»:** المكوّن يحوي ستة أسماء فقط — `height` · `length` · `initial` ·
> `sidewall_r` · `sidewall_l` · `land`. الدرس يذكر `active_target` وهو **غير موجود** فيها، أي أنه
> يُبنى وقت التشغيل. شغّل `DebugSettings ▸ ShowTargets? = True` واقرأ الاسم المرسوم فوق الهدف
> بدل تخمينه.

### ⛔ عقبة مقيسة — `RM_Ledge_Grab` لا يملك حركة جذر

قرأتُ مسار عظم `root` في الكليب: **واحد وثلاثون مفتاحاً كلّها `[0,0,0]`** بدوران وحدة.
الحركة كلّها مخبوزة في الحوض: يتقدّم **+32.6 سم** وينزل **−16.2 سم**.

وإشعار DPTS يستعمل Motion Warping القياسي:

```
Cast To RootMotionModifier_SkewWarp  ─▸  AddOrUpdateWarpTarget
```

و`SkewWarp` **يُحرّف حركة الجذر نحو الهدف**. بصفرٍ لا شيء يُحرَّف — فالمونتاج سيعمل في مكانه
ولن يصل إلى الحافة.

باقي المجموعة سليمة، فالنقص في هذا الكليب وحده:

| الكليب | مسافة الجذر |
|---|---|
| `RM_Ledge_Climb_Up` | 194.6 سم / 1.55 ث |
| `RM_Ledge_Run_L` | 489.8 سم / 0.67 ث |
| **`RM_Ledge_Grab`** | **صفر** |
| GASP `Catch_Cliff_high_stand` | 1101.6 سم / 5.0 ث *(و`force_root_lock = true`)* |

#### مساران، والاختيار قرار فنّي يخصّك

**أ — استخراج حركة الجذر من الحوض** إلى نسخة في `_Project/Parkour/Anims/`.
لكن منحنى الحوض **يندفع ثم يرتدّ**: `+36.6` عند الإطار 3، ثم `+28.5` عند الإطار 10، ثم يستقرّ
على `+32.6`. نقلُ هذا إلى الجذر يُنتج **اندفاعاً ثم ارتداداً للكبسولة** — وهو بالضبط العيب الذي
اشتكيت منه في القفز فوق العوائق. أي أن الحلّ قد يُعيد المرض.

**ب — قصّ `Catch_Cliff_high_stand` عند ثانية واحدة.** يملك حركة جذر حقيقية (1101.6 سم)،
وقياسي السابق يضع لحظة الإمساك عند **0.905**. يُقصّ ما بعدها (التسلّق والوقوف)، ويُفكّ
`force_root_lock`. عيبه أن الحركة من مكتبة GASP لا من مجموعتك.

**المنفَّذ: «ب».** وهذا قياس منحنى سرعة الجذر الذي حدّد نقطة القصّ:

```
0.1 → 1.0   904 → 1488 cm/s     السقوط
1.0 → 1.1   1459 → 297          ← الارتطام
1.2         96                   ← أدنى نقطة: معلَّق
1.3 → 2.9   189 → 73            بدء التسلّق — لا يخصّنا
3.3 → 5.0   0                    واقف
```

النافذة **1.00 → 1.25**، ومسافة الجذر فيها **≈ 44 سم** — داخل `MaxForwardSearchDistance = 90`
فلا شدّ ولا ضغط في الانحراف.

**والقصّ تمّ بأقسام المونتاج لا بجرح الكليب**، لأن طول المونتاج للقراءة فقط ولا يُعاد حسابه
برمجياً (`SequenceLength` read-only، و`CalculateSequenceLength` غير مكشوفة):

| القسم | الزمن | التالي |
|---|---|---|
| `Default` | **1.00** | `None` |
| `Tail` | **1.25** | `None` |

التشغيل يبدأ من `Default` أي من الارتطام، وينتهي عند `Tail`. الثانية الأولى (السقوط، ١١٠٠ سم)
لا تُشغَّل أبداً — وبهذا لا اندفاع.

> **فخّ وجدته:** `WarpTargetName` **ليس خاصّية على الإشعار** بل على كائنه الفرعي
> `RootMotionModifier_SkewWarp`. كل محاولة لضبطه على الإشعار تُرفض بـ`Property not found`.

### إزاحات موحّدة بدل التكرار

`Use State Warp Overrides = True` داخل المونتاج، ثم اضبط الإزاحة **مرّة واحدة** في إعدادات
الشخصية: `Forward = -46` · `Up = 25` *(قيم الدرس — عايرها على شخصيتنا)*.

بهذا لا تُدخل إزاحات يدوية في كل أنيميشن تسلّق لاحقاً.

---

## الخطوة ٣ — صفّ واحد في `DPTS_LedgeCatch_SelectionTable`

الجدول **فارغ تماماً** (`total_rows: 0`)، وبنية صفّه `S_LC_SelectionArray`.
DPTS **لا يشحن أي إعداد مرجعي للالتقاط** — لا يوجد ما يُنسخ، والصفّ يُؤلَّف من الصفر.

| الحقل | القيمة |
|---|---|
| `ActionNameOptional` | `LedgeGrab` |
| `StateMatchRestriction ▸ Target` | `(LedgeClimb, Hang)` = `(NewEnumerator1, NewEnumerator2)` |
| `MontageSelection` | `AM_L2H_Ledge_Grab` |

---

## الحالات والأنواع — مقروءة من التعدادات

```
S_ParkourStates : 0 None · 1 LedgeClimb · 2 PointClimb · 3 FreeClimb · 4 BalanceBeam · 6 Wallrun
S_SubStates     : 0 None · 1 FullContact · 2 Hang
```

> لا يوجد `NewEnumerator5` — حُذف من التعداد. لا تفترض التسلسل.

### من يقرّر: تعلّق أم اتصال كامل؟

`DPTS_Settings_SelectionTable ▸ FromClimbState ▸ SubTypeSettings`:

| النوع | `SubStateSwitchValue` |
|---|---|
| `LedgeClimb` | **80** |
| `BalanceBeam` | **90** |

تحت الحدّ ⇒ `FullContact` (اليدان والقدمان). فوقه ⇒ `Hang` (تعلّق واليدان فقط).

**هذا هو النظام الذي طلبتَه منذ أول يوم — «نظام يقرر هل يصعد او يبقى معلق». موجود ومضبوط.**

### فحوص المساحة لكل حالة

| الحالة | `CharacterSpaceValidationSize` | `TraceDistance` | `BrakingDeceleration` |
|---|---|---|---|
| `LedgeClimb` + `PointClimb`, FullContact | (5, 40, 60) | 60 | 1200 |
| `LedgeClimb`, **Hang** | (5, 40, 90) | 50 | 1200 |
| `BalanceBeam`, Hang | (5, 40, 90) | 40 | 1200 |
| `FreeClimb`, FullContact | (5, 40, 90) | 90 | **2500** |
| `BalanceBeam`, FullContact | — | 60 | 1200 |

سرعة التسلّق القصوى **150** في كل الحالات.

---

## نزاع الدوران مع GASP — نفس فخّ زرّ القفز

من درس **DPTS in GASP** *(تفريغ لإصدار أقدم — أسماء دوالّه لا تنطبق على V3)*:

> «أثناء التسلق يجب تعطيل التدوير، بينما `Update Rotation` في `Event Tick` تعمل باستمرار
> وتفحص `Wants to Strafe`»

GASP يقود دوران الشخصية كل إطار. أثناء حالة الحافة **يستمرّ ويشتبك مع DPTS**، فالشخصية تُدار
بمنطقين متعارضين.

الحلّ بنفس نمط `CanPerformJumpAction` الذي نجح: بوّابة على دالّة الدوران في الأب، شرطها
`ActiveParkourState == None`.

```
ActiveParkourState == None   →  دوران GASP كما هو
ActiveParkourState != None   →  تعطيل الدوران كلياً
```

**لم يُنفَّذ** — يحتاج التحقّق أوّلاً من أن دالّة الدوران في `SandboxCharacter_CMC` عامّة وقابلة
للتجاوز، كما كان `CanPerformJumpAction`. لا تُنفَّذ بالتخمين.

## طبقة الأنيميشن — والمؤلّف يوصي بآلة حالات

نفس الدرس، وهذا الجزء لا يتعلّق بالإصدار:

> «يوصي الشارح باستخدام `State Machine` لحركات التسلق بدلاً من `Motion Matching` لتجنب
> مشاكل `Offset Root Bone`»

البنية التي يصفها:

```
Output Pose  ◂── Blend Poses by Bool
                    False ◂── مسار GASP الطبيعي (Motion Matching)
                    True  ◂── Climb States  (آلة حالات، تتفرّع على S_ParkourStates)
                    bool  ◂── ActiveParkourState != None
```

ودالّة `Set Traversal Variables` (thread-safe) تقرأ المكوّن مع `Is Valid` — الفحص ضروري وإلا
انهارت الإطارات الأولى قبل وجود المكوّن.

**قاعدتنا تبقى:** لا يُعدَّل `SandboxCharacter_CMC_ABP` في مكانه. يُضاعَف مرّة واحدة إلى
`_Project/` ويُبنى عليه، فيبقى أصل Epic سليماً.

> `AnimLayerInterface` في المشروع: **صفر**. و`linked_anim_blueprints` في ABP: **صفر**.
> GASP لا يشحن واجهة طبقات، فالطبقة المنفصلة تحتاج إدراج عقدة داخل ABP الأصلي — ولهذا
> المضاعفة أنظف من «الطبقة» هنا، وتحقّق نفس الهدف.

## ⚠️ فخّ `Force Root Lock` — اتجاهان متعاكسان

| النوع | القيمة | السبب |
|---|---|---|
| **مونتاجات** الانتقال (قفز فوق، التقاط، اعتلاء) | `false` | حركة الجذر هي ما ينقل الشخصية |
| **أنيميشنات وBlendspaces حالة التسلّق** (الوقوف والتحرّك على الحافة) | **`true`** | وإلا انخفض موقع الشخصية أسفل الحافة عند الدمج |

خلط الاثنين يعطي عرضين مختلفين تماماً — لا تعمّم قاعدة واحدة عليهما.

---

## بعد أن يُمسك اللاعب — ما تبقّى بالترتيب

| المرحلة | الأحداث | قيمة المقارنة | كليباتنا الجاهزة |
|---|---|---|---|
| الحركة على الحافة | حالة التسلّق في AnimBP | — | `RM_Ledge_Run_L/R` + `Start/Stop` · `RM_Ledge_Idle` |
| الصعود | `Execute_ClimbStartAction` | `height` | `RM_Ledge_Climb_Up` |
| القفز بين الحواف | `Execute_ClimbJumpAction` | `climb jump distance` | `RM_Ledge_Jump_Up` · `Grab_L/R` |
| الزوايا والانتقالات | `Execute_ClimbTransitionAction` | `length` | *(ناقصة — تحتاج مصدراً)* |
| النزول | `Disengage From State` | — | `RM_Ledge_Drop` |

### إعادة الضبط عند الخروج (`StateResetExecution`)

```
Orient Rotation to Movement = True
Max Walk Speed = 600      Walk Braking Deceleration = 1350
Fly Speed = 600           Fly Braking Deceleration = 0
```

### الحواف المنحنية

أضف `LedgeClimb` إلى `State Transform Correction Allow Listing` ليدور اللاعب تلقائياً حول
الأسطوانات والحواف الدائرية.

---

## ترتيب التنفيذ — لا تقفز خطوة

```
٠  أطفئ صندوق Aura وأعد التشغيل
١  وسم جدار واحد: climb + ledge
٢  LedgeCatch = True  +  ShowTargets? = True
٣  اقفز نحوه — واقرأ اسم الهدف المرسوم
٤  ابنِ AM_L2H_Ledge_Grab على الاسم المقروء لا المخمَّن
٥  صفّ واحد في جدول LedgeCatch
٦  جرّب
```

الخطوات ٠ و١ و٢ **تسبق أي أنيميشن**. ولو رسم النظام هدفاً أخضر فوق الجدار عند الخطوة ٣،
فالطبقة كلّها صارت مسألة ملء جداول.

---

## القاعدة التي كلّفتنا المشروع

**اقرأ الرقم. لا تستنتجه من الاسم.**

`JumpVault` في عرض DPTS شرطه `PrimaryModification = NewEnumerator1`، و`E_GAModList` يعرّفه
**`Sprint`** — لا «قفز». الصفّ ميّت عندنا لأن شخصيتنا لا تضبط `Sprint` أبداً، وما كان ليعمل يوماً.
