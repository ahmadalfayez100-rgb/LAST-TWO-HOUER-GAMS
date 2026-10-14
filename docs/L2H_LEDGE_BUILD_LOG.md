# سجلّ بناء طبقة الحافة — ما غُيِّر ولماذا وكيف تتحقّق

تاريخ الجلسة: 2026-09-21 · الشخصية: `BP_L2H_Base` · **Blueprint فقط**

هذا سجلّ تنفيذ، لا دليل. كل بند فيه: **ما غُيِّر · لماذا · كيف تتحقّق بنفسك · كيف تتراجع.**

---

## ملخّص في سطر

طبقة الحافة صارت مكتملة الحلقة: **إمساك ← تحقّق من المساحة ← تنقّل ← إفلات ← وأنيميشن تعلّق حقيقي**.
لم يُلمس أصل واحد من Epic.

---

## ١ — هندسة الاختبار: `LEDGE_01` و`LEDGE_02`

### ما غُيِّر

| | قبل | بعد |
|---|---|---|
| `LEDGE_01` موقع Z | 160 | **0** |
| `LEDGE_01` مقياس Z | 1.0 | **3.5** |
| `LEDGE_02` موقع Z | 250 | **0** |
| `LEDGE_02` مقياس Z | 1.0 | **3.5** |

كلاهما الآن كتلة من الأرض حتى `Z = 350`، والوسمان `climb` + `ledge` كما كانا.

### لماذا

كانا **لوحين معلّقين في الهواء** سماكتهما 100 وتحتهما فراغ. والشخصية في `FullContact` تجلس ١٢٥ تحت
الحافة — أي **أسفل قاع اللوح**. فمجسّات `ActiveSpaceValidation` تبحث عن سطح تسلّق أمامها فلا تجد
شيئاً، فتَعدّ كل اتجاه «غير صالح» وتمنع الحركة.

هذا هو سبب «لا أتحرّك من مكان المسك» حرفياً — والوثيقة تنصّ عليه:

> `ValidationOffset`: «إن كانت المنطقة غير صالحة، **يتوقّف الممثل عن الحركة في ذلك الاتجاه**»

### كيف تتحقّق

اختر `LEDGE_01` في الـOutliner واقرأ `Transform`: يجب أن تكون `Location Z = 0` و`Scale Z = 3.5`.
وفي المنظر، يجب أن ترى جداراً متّصلاً بالأرض لا لوحاً طائراً.

### التراجع

أعِد `Location Z = 250` و`Scale Z = 1`.

---

## ٢ — جسم جديد: `LEDGE_STEP`

### ما أُضيف

جسم `LevelBlock_C` باسم **`LEDGE_STEP`** عند `x 2100→2400`، `y −100→100`، **`Z 0→175`**.
بلا وسوم، ونصّه الثلاثي الأبعاد أصفر: `STEP 1.75 m`.

### لماذا

الحافة صارت ٣.٥ متر — **خارج مدى الاعتلاء**. وقياسك السابق: «أقصى مدى للقفز ٣ متر».
فبلا درجة وسطى لا تستطيع الصعود فوق الحافة أصلاً لتخرج منها.

الدرجة تجعلها قفزتين: `0 → 175` ثم `175 → 350`. وكلتاهما داخل مدى `HighMantle` (150–350) في
جدول `DPTS_VaultMantle_SelectionTable`.

> **بلا وسوم عمداً** — هي جسم اعتلاء عادي، وليست حافة تتعلّق بها.

### كيف تتحقّق

اكتب `LEDGE` في الـOutliner: يجب أن ترى `LEDGE_01` و`LEDGE_02` و`LEDGE_STEP`.

### التراجع

احذف الجسم. لا شيء يعتمد عليه.

---

## ٣ — وصلة مفقودة في `BP_L2H_Base`

### ما غُيِّر

```
Branch (ActiveParkourState == None) ▸ False  ──▸  Update Parkour State Data ▸ execute
```

### لماذا

كانت **بيانات** العقدة موصولة (`Target` و`RightInput` و`ForwardInput`) و**تنفيذها** غير موصول.
فالدالّة لا تُنادى أبداً ⇒ لا فحص مساحة ⇒ المشي في الفضاء.

هذا النوع من العطب **لا يُنتج خطأ ترجمة ولا تحذيراً** — العقدة تبدو موصولة في الرسم لأن خطوطها
الخضراء موجودة، والمفقود هو الخطّ الأبيض وحده.

### كيف تتحقّق

`validate_blueprint` كان يدرج `Update Parkour State Data` تحت `disconnected_nodes`. الآن لا يدرجها.
وفي الرسم: خطّ أبيض من `False` في الفرع إلى العقدة.

---

## ٤ — ثلاثة كليبات تعلّق جديدة

### ما أُنشئ

| الأصل | النسخة | الطول |
|---|---|---|
| `IP_Ledge_Idle` | **`M_L2H_Ledge_Hang_Idle`** | 2.000 ث |
| `IP_Ledge_Run_L` | **`M_L2H_Ledge_Shimmy_L`** | 0.667 ث |
| `IP_Ledge_Run_R` | **`M_L2H_Ledge_Shimmy_R`** | 0.667 ث |

في `Content/_Project/Parkour/Anims/`، وعلى كلٍّ منها:

```
Loop = True            ← الأصول لم تكن تدور، وحالة التسلّق تحتاج دوراناً
ForceRootLock = True   ← القسم ٩ من الدرس: يمنع انخفاض الشخصية أسفل الحافة عند الدمج
```

### لماذا هذه المجموعة لا حزمة UTA

اخترت **مجموعتك الخاصّة** `Ledge_Animation_Set` لأنها:

- على `SK_UEFN_Mannequin` — **نفس هيكل الشخصية، بلا إعادة استهداف**
- داخل المشروع أصلاً — بلا نقل بين مشروعين وبلا خطر الكتابة فوق آلاف الأصول
- بلا سؤال رخصة معلّق

واخترت نسخ `IP_` (في المكان) لا `RM_` لأن **DPTS يقود الحركة بنفسه**؛ حركة الجذر كانت ستصارعه.

### كيف تتحقّق

افتح أيّاً منها وتحقّق من `Loop` و`Force Root Lock` في `Asset Details`.

### التراجع

احذف الثلاثة. الأصول في `Ledge_Animation_Set` لم تُمسّ.

---

## ٥ — مخطط أنيميشن جديد: `ABP_L2H_Base`

**هذا أكبر تغيير في الجلسة.**

### ما أُنشئ

نسخة من `SandboxCharacter_CMC_ABP` في `Content/_Project/Characters/ABP_L2H_Base`.

### لماذا لم نعدّل أصل GASP

قاعدتنا: **لا يُعدَّل أصل من Epic في مكانه.** وGASP **لا يشحن `AnimLayerInterface`** (فحصته: صفر
واجهات طبقات، وصفر `linked_anim_blueprints`) — فالطبقة المنفصلة كانت ستتطلّب إدراج عقدة داخل
مخطط Epic نفسه. المضاعفة تحقّق الهدف بلا مخالفة.

### التحقّق الذي أجريته قبل الربط

المضاعفة أسقطت وصلات في الشخصية سابقاً، فلم أثق بها. قارنت النسخة بالأصل:

```
compare_blueprints  →  total_diffs: 0     عبر 62 رسماً
```

**صفر فروق.** ولم أربط الشخصية بالمخطط الجديد إلا بعد هذه النتيجة.

### ٥أ — متغيّران جديدان

```
IsClimbing       bool      فئة  L2H | Climb
ClimbInputRight  double    فئة  L2H | Climb
```

### ٥ب — تحديثهما كل إطار

في `EventGraph`، بعد `Update_PropertiesFromCharacter` مباشرةً:

```
Try Get Pawn Owner ──▸ Get Component by Class (DynamicParkourTraceSystem_C)
                          ├─▸ ActiveParkourState ──▸ Equal(None) ──▸ NOT ──▸ Set IsClimbing
                          └─▸ Right Input ─────────────────────────────────▸ Set ClimbInputRight
```

**موضعها مقصود:** وضعتها **قبل** الفرع الذي يفحص `UseThreadSafeUpdateAnimation`، لا بعده.
لو وضعتها بعده لتوقّفت لحظة تفعيلك للتحديث الآمن للخيوط يوماً ما. الآن تعمل في كل الأحوال.

### ٥ج — آلة حالات `ClimbStates`

| الحالة | الأنيميشن |
|---|---|
| `HangIdle` *(المدخل)* | `M_L2H_Ledge_Hang_Idle` |
| `ShimmyRight` | `M_L2H_Ledge_Shimmy_R` |
| `ShimmyLeft` | `M_L2H_Ledge_Shimmy_L` |

ستّة انتقالات بقواعدها:

```
HangIdle    → ShimmyRight    ClimbInputRight  >  0.1
HangIdle    → ShimmyLeft     ClimbInputRight  < −0.1
ShimmyRight → HangIdle       |ClimbInputRight| <  0.1
ShimmyLeft  → HangIdle       |ClimbInputRight| <  0.1
ShimmyRight → ShimmyLeft     ClimbInputRight  < −0.1
ShimmyLeft  → ShimmyRight    ClimbInputRight  >  0.1
```

عتبة **0.1** تمنع الرفرفة عند عصا تحكّم قريبة من الصفر.

### ٥د — المزج عند المخرج
```
Output Pose  ◂── Blend Poses by bool
                   BlendPose_0  ◂── ClimbStates    ← يعمل حين bActiveValue = True
                   BlendPose_1  ◂── Pose History   ← مسار GASP كاملاً
                   bActiveValue ◂── IsClimbing
```

> **⚠️ فخّ كلّفنا جولة:** في `Blend Poses by bool` المقبس **`BlendPose_0` هو «صحيح»**
> و`BlendPose_1` هو «خطأ» — عكس ما يوحي الترقيم. أثبته رسم GASP نفسه: عقدة مزج التصويب
> تضع `BS_Neutral_AO_Stand` على `BlendPose_0` و`Additive Identity Pose` على `BlendPose_1`،
> و`bActiveValue ◂── Enable_AO`.
>
> وصّلتُهما مقلوبَين أول مرّة، فكانت وضعية التعلّق تعمل **طوال الوقت** ما دام اللاعب غير متعلّق.

**وُضع في آخر السلسلة تماماً — وهذا مقصود.** فوضعية التعلّق تتجاوز بذلك:

```
Offset Root Bone  ·  Foot Placement  ·  Leg IK
```

وكلّها معالجات مصمَّمة للوقوف على الأرض. وهي بالضبط ما وصفتَه: «القدم تتحرّك كأنها في لاق»
و«نحيفة ثم طويلة» — لأنها تحاول تثبيت قدمَين على أرض غير موجودة بينما DPTS ينقل الجسم.

> والمؤلّف حذّر من هذا: «يوصي باستخدام `State Machine` لحركات التسلق بدلاً من `Motion Matching`
> لتجنب المشاكل الناتجة عن `Offset Root Bone`».

### ٥هـ — ربط الشخصية

`BP_L2H_Base ▸ Mesh ▸ Anim Class` = **`ABP_L2H_Base_C`**

### كيف تتحقّق

```
افتح BP_L2H_Base ▸ اختر Mesh ▸ Details ▸ Anim Class  =  ABP_L2H_Base_C
```

### التراجع — قائمة منسدلة واحدة

أعِد `Anim Class` إلى `SandboxCharacter_CMC_ABP_C`. **أصل GASP لم يُمسّ إطلاقاً**، فالتراجع فوري
وبلا أثر.

---

## ما لم يُنفَّذ — وأقولها صراحةً

| | لماذا |
|---|---|
| **لم أجرّب اللعبة** | لا أستطيع اللعب. كل ما أعلاه متحقَّق منه **بنيوياً** (ترجمة نظيفة، مقارنة، قراءة من القرص) لا **سلوكياً** |
| `ClimbStart` · `ClimbJump` · `ClimbTransition` · `DropOffAction` | جداولها فارغة، وتحتاج مونتاجات وإشعارات — مرحلة تالية |
| الزوايا | مجموعتك لا تحوي كليبات زوايا. حزمة UTA تحويها، وتحتاج قرار نقل |
| حزمة UTA | **لم تُنقل** — بانتظار رابط الإدراج في `ASSET_LICENSES.md` |
| القفز | يعتمد الآن على `UseJumpOnFail = True` في DPTS. عقدتا عبور GASP باقيتان مفصولتين عمداً |

---

## كيف تجرّب — بهذا الترتيب

**١.** امشِ نحو `LEDGE_STEP` واضغط `Space` ⇒ تعتلي الدرجة (`Z = 175`)
**٢.** من فوقها اضغط `Space` أمام `LEDGE_01` ⇒ تعتلي الحافة (`Z = 350`)
**٣.** امشِ على سطحها واخرج من حافتها الطولية ⇒ **تتعلّق**
**٤.** تحرّك يميناً ويساراً ⇒ **تتنقّل، وبأنيميشن تعلّق لا مشي**
**٥.** واصل حتى الطرف ⇒ **تتوقّف** — لا فضاء
**٦.** اضغط `Space` ⇒ **تسقط، ويعود كل شيء طبيعياً**

### وإن فشلت خطوة

| العرض | انظر أولاً |
|---|---|
| لا تتعلّق | `Priority` في `Anim Selection Ledge Catch` = `StateTypeMatch` |
| تتعلّق وتبدو واقفة | `Anim Class` على `Mesh` = `ABP_L2H_Base_C` |
| تتعلّق ولا تتنقّل | مجسّات `ClimbValidations` في التصحيح — أضفه إلى `SearchTypeSelection` |
| لا تخرج | `Switch ▸ Selection` ◂── `ActiveParkourState` |

---

## القاعدة التي حكمت الجلسة

**قارن بالأصل قبل أن تُنظّر.**

في هذه الجلسة وحدها كشفت المقارنة أربعة أعطال عجز عنها التحليل:

| العطب | كيف كُشف |
|---|---|
| وصلتا النظر بيد التحكّم | مقارنة العقدة بنظيرتها في `SandboxCharacter_CMC` |
| مقياس `Y = 1.97` | مقارنة الـCDO بالأصل |
| `SandboxCharacter_CMC` معدَّل +5,333 بايت | مقارنة بصمة الملفّ بالعيّنة |
| `Update Parkour State Data` مفصولة | `validate_blueprint` |

**ولا واحد منها كان يُنتج خطأ ترجمة.** ولهذا يبقى `sample/GameAnimationSample 5.8` أثمن ملفّ في
المشروع: **هو مسطرتك.**

---
## ٦ — موضع اليد على الحافة: من يقرأ `StateWarpOverrides` فعلاً

**تاريخ: 2026-09-24.** المستخدم: «`Up Offset` لا تعمل»، ثم «حاولت أغيّرها ما تغيّر إشي».
كان محقّاً في الاثنتين، والسبب بنيوي لا عددي.

### ١ — تتبّع السلسلة حتى آخرها

بدأتُ من الحقل وتتبّعت من يقرأه:

```
StateWarpOverrides
   └─ يُقرأ في: Get State Warp Overrides          (قراءتان، لا ثالثة)
        └─ تُستدعى من: ActiveClimbTransformAdjustment
             └─ تُستدعى من: ... لا أحد
        └─ وتُستدعى من: AN_MotionWarp_WithOffset ▸ Received_NotifyBegin   ✅
```

`search_nodes` على المشروع كلّه أعاد **مطابقة واحدة** لـ`ActiveClimbTransformAdjustment`: عقدة
تعريفها نفسها. لا الشخصية تستدعيها، ولا شخصية العرض في DPTS، ولا المكوّن.

**فهي كود ميّت في هذه النسخة.** وكل ما بنيتُه قبل ساعة على قراءتها كان خاطئاً.

### ٢ — المسار الحيّ الوحيد

```
AN_MotionWarp_WithOffset ▸ Received_NotifyBegin
  PrimaryTargetLocal = Make Transform( FindTargetByWarpName("height") , orientation )
  …
  Branch ( UseStateWarpOverrides? )
     ✔ → PrimaryTargetLocal.Location = Offset Vector( الموضع , الاتجاه ,
                                          Forward ◂── GetStateWarpOverrides.ForwardOffset
                                          Up      ◂── GetStateWarpOverrides.UpOffset
                                          Right   ◂── GetStateWarpOverrides.RightOffset )
  → Add or Update Warp Target( Name , Location , Rotation )
```

هنا الإزاحات **مباشرة**: يُزاح هدف الالتفاف بمقدارها عن نقطة الحافة. ولا وجود لبوّابة الصفر
التي رأيتُها في الدالّة الميّتة.

**وهذا يفسّر «ما تغيّر إشي» تفسيراً كاملاً:** القيمة تُقرأ **مرّة واحدة، لحظة بدء مونتاج
الإمساك**. فتغييرها وأنت متعلّق بالفعل لا يفعل شيئاً — المونتاج انتهى. ولأن الدالّة المستمرّة
ميّتة، **لا يوجد تصحيح لكل إطار إطلاقاً**؛ الموضع هو حيث تركك المونتاج.

> **⚠️ المعايرة الحيّة لا تعمل هنا.** الدورة الصحيحة:
> **أوقف اللعب ← غيّر الرقم ← شغّل ← اقفز وأمسك ← انظر.**

### ٣ — الرقم

`Offset Vector` يزيح هدف الالتفاف، وهدف الالتفاف هو **transform الممثّل** في نهاية النافذة.
والقياس من `M_L2H_LedgeClimb_Idle` الإطار 0 (فضاء الشبكة، الجذر المرجعي `(0,0,0)` متحقَّق منه):

```
hand_r  (−16.47 , 36.12 , 127.59)
hand_l  ( 12.37 , 36.14 , 127.48)
المنتصف (−2.05 , 36.13 , 127.53)
```

والشبكة عند `yaw −90` و`Z −88` من الممثّل، أي `actor = (my , −mx , mz − 88)`:

```
اليدان في فضاء الممثّل  =  (36.13 أماماً , 2.05 جانباً , 39.53 فوق الأصل)

ForwardOffset = −36.1     ← تُبعد الأصل 36.1 عن وجه الجدار، فتقع اليدان عليه
UpOffset      = −39.5     ← تُنزل الأصل 39.5 تحت الحافة، فترتفع اليدان إليها
```

### ٤ — ما غُيِّر

| | قبل | بعد |
|---|---|---|
| `[0]` (LedgeClimb+PointClimb, FullContact) Forward | −50 | **−36.1** |
| `[0]` UpOffset | *غائب* | **−39.5** |
| `[1]` (LedgeClimb+BalanceBeam, Hang) Forward | −50 | **−36.1** |
| `[1]` UpOffset | −100 | **−39.5** |

`[2]` FreeClimb (−46) و`[3]` Wallrun (−50) لم تُمسّ. والفهرسان `[0]` و`[1]` جُعلا متطابقَين
لأننا لا نعلم أيّ حالة فرعيّة تنشط عند الإمساك، وأنيميشننا واحد لكلتيهما.

> **أثر جانبي مُعلَن:** `[0]` يطابق `PointClimb+FullContact` و`[1]` يطابق `BalanceBeam+Hang`.
> الحالتان غير مستعملتين (جداولهما فارغة).

وأُضيف `(LedgeClimb, Hang)` إلى `StateTransformCorrectionAllowListing` — **وهو تغيير خامل**
لأنه يبوّب الدالّة الميّتة وحدها. أُبقي عمداً: يوم نُوصّل `ActiveClimbTransformAdjustment`
لتثبيت الموضع أثناء التعلّق، سيكون مطلوباً.

### ٥ — عيب في DPTS يستحقّ التسجيل

`Get State Warp Overrides` تعيد `StateWarpOverrides[IndexLocal]` **حتى حين لا يطابق شيء** —
أي آخر فهرس زارته الحلقة. و`Success?` تكون `false`، **لكن المُشعِر لا يوصّل `Success?` أصلاً.**

فعلى عدم المطابقة تُطبَّق إزاحات **Wallrun** صامتةً. هذا يعني أن رؤية إزاحة «تعمل» لا تُثبت
أن الفهرس الصحيح هو المنتقى.

### ٦ — خطأ ارتكبتُه في نفس اليوم — وسببه

قبل ساعة كتبتُ في هذا الملفّ أن `UpOffset = −127.5`، مشتقّاً من `GetActorRootOffset` الذي
يعيد مستوى القدمين. الاشتقاق كان صحيحاً — **لكن للدالّة الميّتة.** قرأتُ مستهلِكاً واحداً
وافترضتُ أنه الوحيد، ولم أسأل **«من يستدعي هذه الدالّة؟»** إلا بعد أن قال المستخدم إن التغيير
لا يفعل شيئاً.

**القاعدة:** تتبّع السلسلة إلى **آخرها**. قراءة من يقرأ الحقل لا تكفي؛ اسأل من يستدعي القارئ.

### كيف تتحقّق

```
BP_L2H_Base ▸ DynamicParkourTraceSystem ▸ System | Overrides ▸ State Warp Overrides
   Index [0] و[1] ▸ Offset Values  =  Forward −36.1 , Up −39.5
```

مقروء من المحرّك بعد الحفظ (لا من نتيجة الكتابة). على القرص: `BP_L2H_Base.uasset` = **1,509,223** بايت.

### التراجع

```
[0]  Forward −50 ، احذف UpOffset
[1]  Forward −50 ، UpOffset −100
StateTransformCorrectionAllowListing  احذف العنصر الرابع
```

---

## ٧ — تحذيرات `LogChooser`: 100,151 خطأ من أصل 100,160

### التشخيص

```
Error: Chooser Table: CHT_PoseSearchDatabases_Dense  ContextData entry 0 expects …   48,782
Error: Chooser Table: CHT_PoseSearchDatabases        ContextData entry 0 expects …   48,782
Error: Chooser Table: CHT_CMCCharacterAnimations     ContextData entry 0 expects …    2,587
```

الرسالة مبتورة في الشاشة. والنوع المطلوب ليس الشخصية بل **مخطط الأنيميشن**:

```
ContextData[0].Class = /Game/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP_C
```

و`ABP_L2H_Base` **مضاعفة** أبوها `AnimInstance` — أي أخت لا ابنة، فلا تُطابق النوع.

**الأثر:** الجداول تسقط على صفّها الافتراضي كل إطار، فاختيار قواعد Motion Matching حسب
السياق معطّل، والسجلّ يبتلع كل ما عداه.

### لماذا لم يكن الحلّ تعديل جداول GASP

قاعدتنا: **لا يُعدَّل أصل من Epic في مكانه.** ولا ينفع توسيع النوع إلى `AnimInstance` لأن
ارتباطات الأعمدة (`MMDatabaseLOD` · `Stance` · `Gait` · `MovementMode` · `MovementState`)
تُحلّ بالصنف.

### ما نُفِّذ

**أ — مضاعفة الشجرة** إلى `Content/_Project/Characters/Choosers/`:

```
CHT_PoseSearchDatabases                ← الجذر، أُعيد توجيه 4 إحالات داخليّة
CHT_PoseSearchDatabases_Dense
CHT_PoseSearchDatabases_Sparse
CHT_PoseSearchDatabases_ExtremeSparse
CHT_CMCCharacterAnimations             ← جداوله الفرعيّة (48) داخله، فجاءت معه
```

**ب — تغيير نوع السياق** على الخمسة إلى `ABP_L2H_Base_C`.

**ج — توجيه العقدتين** في `ABP_L2H_Base`:

```
Update_MotionMatching        ▸ Evaluate Chooser  →  Choosers/CHT_PoseSearchDatabases
SetBlendStackAnimFromChooser ▸ Evaluate Chooser  →  Choosers/CHT_CMCCharacterAnimations
```

تغيير الجدول أعاد بناء العقدة، فتبدّل نوع مقبس السياق وصار المقبس القديم يتيماً.
**وقبل إعادة التوصيل قارنتُ بالأصل:**

| | الأصل `SandboxCharacter_CMC_ABP` | ما فعلتُه |
|---|---|---|
| `Update_MotionMatching` | المقبس موصول بـ`Self` | وصّلته بـ`Self` |
| `SetBlendStackAnimFromChooser` | المقبس **فارغ** | تركته فارغاً |

لولا المقارنة لوصّلت الثاني «إصلاحاً» وخالفت تصميم Epic. ترجمة نظيفة: **0 أخطاء، 0 تحذيرات.**

### كيف تتحقّق

```
افتح ABP_L2H_Base ▸ Update_MotionMatching ▸ عقدة Evaluate Chooser
   يجب أن تقرأ: Evaluate Chooser: CHT_PoseSearchDatabases  ومقبسها  ABP_L2H_Base_C
```

وبعد التشغيل: `Saved/Logs/lastTwoHours.log` يجب أن يخلو من `LogChooser … expects`.

**أصول GASP لم تُمسّ** — تاريخ تعديلها ما زال `09-03 08:05`.

### ما تبقّى

مقبس يتيم بلا وصلات على عقدة `Update_MotionMatching` — لا يُنتج تحذيراً ويزول عند أول تحديث
للعقدة. متروك عمداً بدل مخاطرة بإعادة بناء يدويّة.

### التراجع

أعِد الجدولين على العقدتين إلى أصلي GASP، واحذف مجلّد `_Project/Characters/Choosers/`.

### وبقيّة السجلّ

بعد الجداول لا يبقى شيء يُذكر: تحذيرات مصادقة Aura وخادمها، و`Using CommonUI without a
CommonGameViewportClient` ×10، و`Unable to load LTHLedgeHangMode_` — بقيّة طبقة C++ المحذوفة،
و`UStateTreeComponent … State Tree asset is not set`. لا شيء منها يخصّ الحافة.

---

## ٨ — لماذا `UpOffset` لم تعمل: مفتاحان في المحرّك لا في DPTS

**تاريخ: 2026-09-24.** المستخدم بعد تجربتين: «`ForwardOffset` يعمل وضبطته على −55،
لكن `UpOffset` جرّبت −250 و0 وكان مكان التعليق متشابهاً».

محورٌ يعمل ومحورٌ لا — هذه ليست مشكلة رقم ولا مشكلة اختيار فهرس. فقرأتُ المُشعِر نفسه.

### ما وجدتُه

```
AM_L2H_LedgeCatch_Hang ▸ AN_MotionWarp_WithOffset_C_0 ▸ RootMotionModifier_SkewWarp

   warp_target_name       = height
   ignore_z_axis          = True     ← ✘ المشكلة
   warp_to_feet_location  = True     ← ✘ سبب خطأ الرقم
   warp_translation       = True
   warp_rotation          = True
```

**`ignore_z_axis = True`** هو الافتراضي في `URootMotionModifier_Warp` في المحرّك. معناه أن
الالتفاف **يُسقط محور Z من هدفه تماماً** ويترك الارتفاع لحركة جذر الأنيميشن.

فـ`Offset Vector` في المُشعِر يحسب `UpOffset` بأمانة، ويُمرَّر الهدف إلى
`Add or Update Warp Target` كاملاً — ثم **يرميه المحرّك**. ولهذا `−250` و`0` أعطيا النتيجة
نفسها بالضبط: كلاهما مُهمَل.

وأنيميشن الإمساك عندنا تسع إطارات مقتطعة من سقوط، فحركة جذرها لا تضبط الارتفاع أصلاً.

### `warp_to_feet_location` — ويصحّح الرقم

المفتاح الثاني يقول إن الهدف يُطابَق بـ**القدمين** لا بمركز الكبسولة. فـ`UpOffset` في فضاء
القدمين:

```
اليدان فوق عظم الجذر (= القدمين)  =  127.53
handsZ = ledgeZ   ⇔   feetZ − ledgeZ = −127.53

UpOffset = −127.5
```

وهذا يطابق ما اشتُقّ من `GetActorRootOffset` في القسم ٦ — **فDPTS يقيس من القدمين في
المسارين معاً**، والخطأ كان في تحويلي إلى فضاء أصل الممثّل بفارق 88 (نصف الكبسولة).

### ما غُيِّر

**أ — على المونتاج** `AM_L2H_LedgeCatch_Hang` (أصل مملوك، لا من Epic ولا من DPTS):

```
RootMotionModifier_SkewWarp ▸ ignore_z_axis :  True  →  False
```

**ب — على الشخصية:**

| | قبل | بعد |
|---|---|---|
| `[0]` Forward | **−55** *(ضبط المستخدم)* | **−55** — أُبقي |
| `[0]` Up | *غائب* | **−127.5** |
| `[1]` Forward | **−55** *(ضبط المستخدم)* | **−55** — أُبقي |
| `[1]` Up | *غائب* | **−127.5** |

> قراءة `old_value` أثبتت أن `−55` كان على الفهرسين معاً، وأن `UpOffset` كان **محذوفاً
> كليّاً** — وضعُ الحقل على `0` يجعل المحرّك يُسقط المفتاح. فتجربة «0» لم تكن تجربة صفر
> بل تجربة غياب.

### كيف تتحقّق

```
افتح AM_L2H_LedgeCatch_Hang ▸ اختر مُشعِر AN_MotionWarp_WithOffset
   Details ▸ Root Motion Modifier ▸ Ignore Z Axis  =  ✗ غير مؤشَّر
```

مقروء من المحرّك بعد الحفظ. على القرص:
`AM_L2H_LedgeCatch_Hang.uasset` = **13,702** بايت · `BP_L2H_Base.uasset` = **1,509,234** بايت.

### التراجع

أعِد `Ignore Z Axis = True` واحذف `UpOffset` من الفهرسين.

### الدرس

ثلاث جولات على هذا الرقم، وكلّها فشلت لأني بقيت داخل DPTS. والحقيقة أن **DPTS كان يحسب
الرقم صحيحاً منذ البداية، والمحرّك هو الذي يرميه.**

> **القاعدة:** حين يعمل محور ولا يعمل الآخر، فالعطب ليس في القيمة ولا في المنطق الذي
> يولّدها — بل في المستهلِك النهائي. اقرأ إعدادات العقدة التي تستلم النتيجة **قبل** أن تعيد
> اشتقاق الرقم.

---

## ٩ — قياس الوضعيّة نفسها: لماذا لا تُصلح الإزاحة اليدَ وحدها

**تاريخ: 2026-09-24.** المستخدم: «الرِّجل في المكان الصحيح بس الإيد لا».

### القياس — لا تخمين

قرأتُ عظام الكليبات الثلاثة في فضاء المكوّن (`AnimPoseExtensions.get_anim_pose_at_frame`).
المحاور: `mesh Y = أمام الممثّل` · `mesh X = الجانب` · `Z = فوق عظم الجذر`.

| الكليب | `hand` أمام | `hand` فوق | `calf_r` أمام | `foot_r` فوق |
|---|---|---|---|---|
| `M_L2H_LedgeClimb_Idle` | 17.2 | **128.2** | 17.6 | 5.7 |
| `M_L2H_LedgeClimb_Right` | 18.4 | **128.4** | 17.2 | — |
| `M_L2H_LedgeClimb_Left` | 18.5 | **136.5** | 16.6 | — |

**اليد والساق على البُعد الأمامي نفسه ≈ 18.** فالكليب مُؤلَّف لجدار عمودي: الكفّان على الحافة
والساقان ملاصقتان للجدار تحتها. وضعيّة متماسكة فيزيائيّاً.

### الاستنتاج

```
ForwardOffset = −18      ← يضع الكفّين والساقين على مستوى الجدار معاً
UpOffset      = −128.4   ← يضع الكفّين على سطح الحافة
```

والقيمة التي كانت `−53` تُبعد الجسم 53 عن الجدار، أي **35 سم قبله** — وهذا حرفيّاً ما تُظهره
الصورة: يدان في الهواء لا تلمسان شيئاً.

> **القاعدة التي تفسّر السؤال:** الإزاحة تحرّك **الجسم كلّه**. واليد والقدم مربوطتان ببعضهما
> داخل الكليب برباط ثابت. فلا يوجد رقم يُصلح إحداهما دون الأخرى. إمّا أن تكون الوضعيّة نفسها
> صحيحة للجدار — وهي كذلك هنا — أو تحتاج `Two Bone IK` على `hand_l/hand_r` لتثبيت الكفّين
> بمعزل عن الجسم.

### عيب مقيس في الكليبات

`Left` يتعلّق أعلى بـ**8.1 سم** من `Idle` و`Right` (136.5 مقابل 128.3). فعند الانتقال إلى
`ShimmyLeft` ستقفز الشخصية 8 سم لأعلى. غير مُصلَح — يحتاج تحرير الكليب أو إزاحة `Translation
Retargeting` على الجذر.

### سرعة الأنيميشن وسرعة اللاعب

سفر الجذر المُؤلَّف في الكليبين (`root` جانبيّاً، من الإطار 0 إلى 40 خلال 1.333 ث):

```
Left    1.39 →  77.98   =  76.6 سم  ⇒  57.4 سم/ث
Right   0.19 → −72.14   =  72.3 سم  ⇒  54.3 سم/ث
                                المتوسّط  ≈  55.9 سم/ث
```

وسرعة اللاعب في `DPTS_Settings_SelectionTable ▸ FromClimbState ▸ PrimarySettings`:

```
[0] LedgeClimb+FullContact , PointClimb+FullContact   MaxSpeed = 200
[1] LedgeClimb+Hang                                   MaxSpeed = 150
```

فالجسم يقطع 150 سم/ث والأنيميشن يصوّر 56 — انزلاق بنسبة **2.7×**، وهو سبب «الحركة مبالغ فيها».

**المعادلة:**

```
PlayRate  =  MaxSpeed  /  55.9
```

| الشعور | MaxSpeed | PlayRate |
|---|---|---|
| طبيعي تماماً — بلا تسريع | 56 | 1.00 |
| **المطبَّق** | **90** | **1.61** |
| سريع | 150 | 2.68 |

### ما غُيِّر

| الملفّ | الحقل | قبل | بعد |
|---|---|---|---|
| `BP_L2H_Base` | `StateWarpOverrides[0]` و`[1]` Forward | −53 | **−18** |
| `BP_L2H_Base` | نفسهما Up | −140 | **−128.4** |
| `ABP_L2H_Base` | `ClimbStates/ShimmyRight ▸ PlayRate` | 1.0 | **1.61** |
| `ABP_L2H_Base` | `ClimbStates/ShimmyLeft ▸ PlayRate` | 1.0 | **1.61** |
| `DPTS_Settings_SelectionTable` | `[0]` MaxSpeed | 200 | **90** |
| `DPTS_Settings_SelectionTable` | `[1]` MaxSpeed | 150 | **90** |

> `[2]` BalanceBeam و`[3]` FreeClimb و`[4]` BalanceBeam (150) وWallrun (850) **لم تُمسّ** —
> متحقَّق منها بقراءة عكسيّة بعد الكتابة.
>
> **`−53 / −140` كانا ضبطك بالنظر، وقد استبدلتُهما.** كنتَ تعوّض بالارتفاع عن خطأ في البُعد
> الأمامي. لو بدا `−140` أفضل بعد التجربة فأعِده — لكن جرّب `−18` أوّلاً.

### كيف عُدِّل جدول DPTS

`DPTS_Settings_SelectionTable` أصل من طرف ثالث. عُدِّل عبر تصدير JSON ← تحرير حقلين ←
إعادة استيراد، لا بالكتابة اليدويّة على سلسلة ضخمة. وقُرئ بعدها للتأكّد:

```
MaxSpeed: 90 , 90 , 150 , 150 , 150 , 850
```

### كيف تغيّرهما بنفسك

**سرعة الأنيميشن:**
```
ABP_L2H_Base ▸ AnimGraph ▸ ClimbStates ▸ ShimmyLeft / ShimmyRight
   اختر عقدة Sequence Player ▸ Details ▸ Play Rate
```

**سرعة اللاعب:**
```
DPTS_Settings_SelectionTable ▸ NewRow ▸ FromClimbState ▸ PrimarySettings
   Index [0] و[1] ▸ SpeedSettings ▸ Max Speed
```

**غيّرهما معاً دائماً** بنسبة `PlayRate = MaxSpeed / 55.9`، وإلا عاد الانزلاق.

على القرص: `BP_L2H_Base` **1,509,234** · `ABP_L2H_Base` **4,252,059** ·
`DPTS_Settings_SelectionTable` **19,948** بايت.

---

## ١٠ — مسح `AdvancedTraversalSystem`: هل نستبدل كليبات الحافة؟

**تاريخ: 2026-09-24.** طلب المستخدم مسح ATS بدل الاستمرار على «أنيميشن مصنوع لغرض ثانٍ».

### الخلاصة في سطر

**نعم، يستحقّ.** بنية ATS تُطابق بنية DPTS مصطلحاً بمصطلح، وكليباته مُؤلَّفة للجدار فعلاً —
وكليبنا الحالي ليس كذلك، وهذا مقيس لا مُدَّعى.

### البنية — تطابق دقيق مع DPTS

| ATS | DPTS |
|---|---|
| `Braced` | `S_SubStates` = **`FullContact`** — القدمان على الجدار |
| `Freehang` | `S_SubStates` = **`Hang`** — الجسم معلّق حرّاً |

وهذا هو بالضبط الانقسام الذي يحكمه `SubStateSwitchValue = 80` في
`DPTS_Settings_SelectionTable`. لا حاجة لأي ترجمة بين النظامين.

### المقارنة المقيسة — لماذا كليبنا لا يصلح

فضاء المكوّن، الإطار 0، `Z` فوق عظم الجذر:

| | اليد أمام | **اليد فوق** | `calf` أمام | القدم فوق |
|---|---|---|---|---|
| **كليبنا** `M_L2H_LedgeClimb_Idle` | 17.2 | **128.2** | 17.6 | 5.7 |
| ATS `Braced/MD_hang_idle` | 21.1 | **167.0** | 7.9 | **59.6** |
| ATS `Freehang/MD_bar_hang_idle` | 21.1 | **167.0** | 27.0 | **−25.7** |

**الفرق 39 سم في ارتفاع اليد.** كليبنا ذراعاه مثنيّتان والجسم مكوَّر — وهذه هي الوضعيّة
التي رآها المستخدم في الصورة. وATS ذراعاه ممدودتان فوق الرأس: تعلّق حقيقي.

وانظر القدم: في `Braced` عند **+59.6** أي مرفوعة على الجدار، وفي `Freehang` عند **−25.7**
أي متدلّية تحت الجذر. الكليبان مُؤلَّفان للحالتين بوعي. وكليبنا عند **+5.7** — واقف تقريباً.

### السرعة

| | المسافة | المدّة | السرعة |
|---|---|---|---|
| كليبنا `M_L2H_LedgeClimb_Left` | 76.6 | 1.333 ث | **57.4 سم/ث** |
| ATS `MD_Shimmy_L` | 70.0 | 0.667 ث | **105.0 سم/ث** |
| ATS `MD_Shimmy_02_L` / `_03_L` | 150.0 | 1.900 ث | **78.9 سم/ث** |

وATS يشحن `Shimmy_BS` — **BlendSpace أحاديّ المحور** يمزج الثلاثة حسب السرعة. أي سرعة
متغيّرة جاهزة بدل `PlayRate` ثابت. و`MaxSpeed = 90` عندنا يقع بين 79 و105 — في قلب المدى.

### تكلفة الاستيراد: إعادة استهداف واحدة

```
ALS_Mannequin_Skeleton  ≟  UE4_Mannequin_Skeleton
    68/68 عظمة · hierarchy_matches = true          ← مطابق تماماً

ALS_Mannequin_Skeleton  ≟  SK_UEFN_Mannequin
    68 مقابل 91 · missing_in_b = []                ← كل عظامه موجودة في هدفنا
```

فالمسار هو UE4→UE5 القياسي. والمشروع يملك طرفيه أصلاً:

```
IK_UE4_Mannequin_Retarget    /Game/Characters/UE4_Mannequin/Rigs/    ← المصدر
IK_UEFN_Mannequin            /Game/Characters/UEFN_Mannequin/Rigs/   ← الهدف
RTG_UEFN_to_UE4_Mannequin                                            ← الاتجاه المعاكس، موجود
```

**المطلوب: `RTG_ALS_to_UEFN` واحد**، ثم استهداف دفعي للكليبات المختارة فقط.

### القائمة القصيرة — 14 كليباً تكفي للحلقة الحاليّة

```
Idle/Braced/MD_hang_idle                 → HangIdle   (FullContact)
Idle/Freehang/MD_bar_hang_idle           → HangIdle   (Hang)
Shimmy/Braced/MD_Shimmy_L , _R           → ShimmyLeft / ShimmyRight
Shimmy/Braced/MD_Shimmy_02_L , _R        → طبقة السرعة الثانية
Shimmy/Freehang/MD_Bar_Shimmy_L , _R     → التنقّل في وضع Hang
Grab/Braced/MD_grab_from_air             → يحلّ محلّ M_L2H_LedgeCatch_A المقتطَع
Grab/Freehang/MD_Bar_Grab
Release/MD_release , MD_bar_release      → الإفلات — جدول DropOffAction الفارغ
Corner/Braced/MD_corner_inner_l , _outer_l → الزوايا الناقصة عندنا
```

> `MD_grab_from_air` مهمّ بذاته: كليب إمساك **مُؤلَّف للغرض**، بديل عن تسع إطارات اقتطعتُها
> من أنيميشن سقوط. ويده عند `fwd −49.3` `up 129.8` — أي يبدأ بعيداً ويصل، وهو سلوك إمساك صحيح.

### ما يفتحه أيضاً — الجداول الستّة الفارغة

| جدول DPTS فارغ | مصدره في ATS |
|---|---|
| `ClimbStart` | `Climbing_System/Animations/Climb` + `Montages/*climb_up*` |
| `ClimbJump` | `Leap` (+ `Leap_to_Ledge_*_BS`) |
| `ClimbTransition` | `Corner` · `Reach` · `SwitchSide` |
| `DropOffAction` | `Release` |
| `WallrunJump` | `WallRunning/Animations` — 85 كليباً |
| *(BalanceBeam)* | `NarrowPath/Animations` — 84 كليباً |

إجماليّ مجلّد التسلّق: **452 أصلاً**.

### ما لا يُؤخذ

`Climbing_System/Blueprints` و`Animations/Overlays` (أوضاع المسدّس) و`Audio`. نأخذ
**الأنيميشن فقط** — منطق الباركور عندنا DPTS، ولا نريد نظامين متنافسين على نفس الشخصية.

### الالتزام قبل النقل

ATS **مدفوع** (MD6_Products). محدَّث في `ASSET_LICENSES.md`، وكان مسجَّلاً خطأً على أنه
«خارج `Content` — غير مستعمل»؛ هو الآن داخل `Content/AdvancedTraversalSystem/`.

> **تبعيّة يجب الانتباه لها:** ATS يعتمد على `Content/AdvancedLocomotionV4/` لهيكله.
> لا يُحذف ALS قبل إتمام إعادة الاستهداف.

### لم يُنفَّذ شيء

هذا مسح وقرار، لا هجرة. إعادة الاستهداف والاستبدال لم تبدأ.

---

## ١١ — تنفيذ الهجرة: `RTG_ALS_to_UEFN` و١٤ كليباً ودمجها

**تاريخ: 2026-09-24.**

### ١ — مخطّط المصدر

`IK_UE4_Mannequin_Retarget` مربوط بهيكل UE4، لا بهيكل ALS. فضُوعف ووُجِّه:

```
IK_UE4_Mannequin_Retarget  ──نسخة──▸  /Game/_Project/Characters/Rigs/IK_ALS_Source
     preview mesh → AdvancedLocomotionV4/.../Meshes/Mannequin
     skeleton     → ALS_Mannequin_Skeleton          ✔ متحقَّق بالقراءة
     chains       → 22 سلسلة، كلّها سليمة بعد المضاعفة
```

**لم يُمسّ أصل Epic.** والمضاعفة هنا آمنة لأن هيكلي ALS وUE4 متطابقان 68/68 بتسلسل مطابق.

### ٢ — المُعيد

```
/Game/_Project/Characters/Rigs/RTG_ALS_to_UEFN
   source = IK_ALS_Source          target = IK_UEFN_Mannequin
   auto_map = fuzzy
   ⇒ 30 سلسلة هدف مربوطة، صفر سلسلة معلّقة
   ⇒ 5 عمليّات: Pelvis Motion · FK Chains · Run IK Rig · Root Motion · Remap Curves
```

الأصابع الزائدة في UEFN (`*_metacarpal_*`) رُبطت تلقائيّاً بسلاسل الأصابع المقابلة في ALS.

### ٣ — التحقّق من جودة الاستهداف

قِيست عظام مطابقة في المصدر والناتج، الإطار 0:

| الكليب | القدم Δأمام | القدم Δفوق | اليد Δأمام | **اليد Δفوق** |
|---|---|---|---|---|
| `hang_idle` | +0.2 | −0.4 | −0.2 | **−14.5** |
| `bar_hang_idle` | −1.3 | +1.4 | +0.4 | **−14.1** |
| `Shimmy_L` | +0.1 | −0.7 | +0.4 | **−13.6** |
| `Shimmy_R` | +0.2 | −0.5 | +0.3 | **−13.5** |
| `Bar_Shimmy_L` | −1.3 | +1.0 | +2.2 | **−13.8** |
| `grab_from_air` | +2.8 | +0.3 | +2.3 | **−12.2** |

القدمان تطابقان ضمن ١ سم. واليدان أوطأ بـ**13.5–14.5 سم بانتظام** — وهذا **فرق نسب الذراعين**
بين UE4 وUEFN، لا عطب استهداف: لو كان عطباً لتشتّت الرقم.

وحتى بعد هذا الفقد، اليد عند **152.5** مقابل **128.2** في كليبنا القديم — **أعلى بـ24 سم**.

### ٤ — أيّ مجموعة تُربط: حُسم من الكود لا بالتخمين

```
CalculateSubType  ▸  Select_0.NewEnumerator1 (LedgeClimb)  ◂──  Select_11
    Select_11.Index = ( GetClimbAreaHeightF  >  SubTypeSettings.SwitchValue )
        Option 1 (صحيح) = NewEnumerator1 = FullContact
        Option 0 (خطأ)  = NewEnumerator2 = Hang
```

و`SwitchValue = 80` لـ`LedgeClimb`. وجدارنا `LEDGE_01` صُلب من `Z 0` إلى `350` ⇒ ارتفاع
منطقة التسلّق **350 ≫ 80** ⇒ **`FullContact`** ⇒ المجموعة الصحيحة هي **`Braced`**.

كليبات `Freehang` مُستهدَفة ومحفوظة وجاهزة ليوم تظهر حالة `Hang` حقيقيّة (حافة معلّقة/عارضة).

### ٥ — ما رُبط

| حالة `ClimbStates` | قبل | بعد |
|---|---|---|
| `HangIdle` | `M_L2H_LedgeClimb_Idle` | **`L2H_MD_hang_idle`** |
| `ShimmyRight` | `M_L2H_LedgeClimb_Right` | **`L2H_MD_Shimmy_R`** |
| `ShimmyLeft` | `M_L2H_LedgeClimb_Left` | **`L2H_MD_Shimmy_L`** |

وعلى الكليبات الثمانية الدوّارة: `Loop = True` · `Force Root Lock = True`.

### ٦ — السرعة، مُعاد اشتقاقها للكليب الجديد

```
L2H_MD_Shimmy_L / _R    dx = ±67.37 سم  خلال 0.667 ث   ⇒  101.1 سم/ث
L2H_MD_Shimmy_02_L / _R dx = ±144.37    خلال 1.900 ث   ⇒   76.0 سم/ث

PlayRate = MaxSpeed / 101.1 = 90 / 101.1 = 0.89
```

| | قبل | بعد |
|---|---|---|
| `ShimmyLeft` · `ShimmyRight` ▸ PlayRate | 1.61 | **0.89** |

الكليب الجديد **أسرع من حاجتنا** بدل أن يكون أبطأ — فصار التصحيح تبطيئاً طفيفاً (0.89)
بدل تسريع 1.61. وهذا وحده يُزيل «الحركة المبالغ فيها».

> `L2H_MD_Shimmy_L` و`_R` متماثلان تماماً (±67.37) — بخلاف كليبينا القديمين (76.6 مقابل 72.3).

### ٧ — الإزاحة، مُعاد اشتقاقها

من `L2H_MD_hang_idle` الإطار 0: `hand_r fwd = 20.90` · `up = 152.51`.

| | قبل | بعد |
|---|---|---|
| `[0]` FullContact Forward | −18.0 | **−20.9** |
| `[0]` FullContact Up | −128.4 | **−152.5** |
| `[1]` Hang Forward | −18.0 | **−21.5** |
| `[1]` Hang Up | −128.4 | **−152.9** |

الفهرسان لم يعودا متطابقَين: لكلّ حالة فرعيّة كليبها الخاصّ الآن، فلكلٍّ رقمها المقيس منه.

### ٨ — خلل وقع أثناء التنفيذ

بعد الاستهداف حفظتُ الكليبات الثمانية التي غيّرتُ أعلامها فقط. وفحص القرص أظهر **8 ملفّات
لا 14** — فالستّة الأخرى (`grab` · `bar_grab` · `release` ×2 · `corner` ×2) كانت في الذاكرة
ولم تُكتب. حُفظت، وأُعيد الفحص: **14/14**.

> `batch_retarget_animations` يُنشئ الأصول لكنه لا يضمن كتابتها. **عُدَّ الملفّات على القرص.**

### كيف تتحقّق

```
ABP_L2H_Base ▸ AnimGraph ▸ ClimbStates ▸ كل حالة
   HangIdle    → L2H_MD_hang_idle        (تحقَّق: get_state_info)
   ShimmyLeft  → L2H_MD_Shimmy_L   PlayRate 0.89
   ShimmyRight → L2H_MD_Shimmy_R   PlayRate 0.89

BP_L2H_Base ▸ DPTS ▸ State Warp Overrides ▸ [0] = −20.9 / −152.5
```

على القرص: 14 كليباً في `Content/_Project/Parkour/Anims/ATS/` ·
`RTG_ALS_to_UEFN` 25,495 · `IK_ALS_Source` 87,417 ·
`ABP_L2H_Base` 4,251,942 · `BP_L2H_Base` 1,508,290 بايت.
ترجمة نظيفة: 0 أخطاء، 0 تحذيرات.

**أصول ATS وEpic لم تُمسّ** — تواريخها `09-24 19:03` و`09-03 08:05`.

### التراجع

أعِد الحالات الثلاث إلى `M_L2H_LedgeClimb_Idle/Left/Right`، و`PlayRate` إلى 1.61،
والإزاحة إلى −18 / −128.4. الكليبات القديمة لم تُحذف.

### ما لم يُنفَّذ بعد

| | لماذا |
|---|---|
| استبدال مونتاج الإمساك بـ`L2H_MD_grab_from_air` | المونتاج الحالي 0.267 ث والكليب الجديد 1.333 ث — يحتاج إعادة تأليف للمُشعِرات، وهو تغيير مستقلّ |
| جداول `DropOffAction` · `ClimbTransition` | الكليبات مُستهدَفة وجاهزة (`release` · `corner`) لكن الجداول لم تُملأ |
| مجموعة `Freehang` | مُستهدَفة ومحفوظة، وغير مربوطة — لا حالة `Hang` في مستوانا الحالي |
| **لم أجرّب اللعبة** | كل ما أعلاه متحقَّق منه بنيويّاً وبالقياس، لا سلوكيّاً |

---

## ١٢ — الجسم ينزاح ثم يقفز راجعاً: الجذر كان فارغاً

**تاريخ: 2026-09-24.** المستخدم بعد ضبط التمركز: «اللاعب لا يتحرّك بقدر الأنيميشن، ما ينتج
أن الأنيميشن بعد أن ينتهي اللاعب يعود للخلف».

### التشخيص

قرأتُ مسارَي `root` و`pelvis` في `L2H_MD_Shimmy_L`:

```
f0    root.x = 0.00   pelvis.x =  1.49   hand_r.x =  −8.30
f20   root.x = 0.00   pelvis.x = 42.21   hand_r.x =   3.00
f40   root.x = 0.00   pelvis.x = 68.86   hand_r.x =  59.08
```

**الجذر مسطّح عند صفر في كل إطار.** وكل الانزياح — 67.37 سم — على الحوض وما تحته.

فـ`Force Root Lock = True` **لا يفعل شيئاً**: لا يوجد جذر ليُثبَّت. والنتيجة أن الكبسولة
تتحرّك بـ`MaxSpeed` **والجسم ينزاح 67 سم فوقها**، ثم تُعاد الدورة فيقفز الجسم راجعاً.
وهذا حرفيّاً وصف المستخدم.

### السبب

في مصدر ATS كان `root.x` يحمل الانزياح (70.00) والحوض ثابت نسبةً إليه. وعمليّة
`Root Motion` في المُعيد جاءت مضبوطةً هكذا:

```
IKRetargetRootMotionOp
   SourceRoot = pelvis        ← ✘ يجب أن تكون root
   TargetRoot = pelvis        ← ✘
   RootMotionSource = CopyFromSourceRoot
```

فنَسخت «حركة الجذر» من الحوض إلى الحوض، وتركت عظم `root` فارغاً.

حاولتُ تصحيح الحقلين عبر `IKRetargetRootMotionController.set_settings` — **لم يثبت**:
القراءة بعد الكتابة أعادت `pelvis`. فلم أُصِرّ عليه.

### الإصلاح المطبَّق

نقلتُ الانزياح إلى الجذر مباشرةً على الكليبات الستّة، مع **الحفاظ التامّ على الوضعيّة
في الفضاء العالمي**:

```
ramp(f)        =  (pelvis.x[N] − pelvis.x[0]) · f/N
root.x(f)     +=  ramp(f)
pelvis.x(f)   −=  ramp(f)
```

المجموع ثابت ⇒ لم يتغيّر شيء في الشكل، وتغيّر **من يحمل الحركة** فقط.
وY وZ والدوران والمقياس لم تُمسّ.

| الكليب | الانزياح المنقول |
|---|---|
| `L2H_MD_Shimmy_L` / `_R` | ±67.37 |
| `L2H_MD_Shimmy_02_L` / `_R` | ±144.37 |
| `L2H_MD_Bar_Shimmy_L` / `_R` | ±67.37 |

### التحقّق

```
              root.x    pelvis.x(local)   world hand.x   hand نسبةً للجسم
f0             0.00          1.49            −8.30           −8.30
f20           33.69          8.53              3.00          −30.69
f40           67.37          1.49             59.08           −8.30
```

**سطران يُثبتان الصحّة:**

1. `world hand.x` = `−8.30 → 3.00 → 59.08` — **مطابق تماماً لما كان قبل التعديل**،
   فالأنيميشن لم يتغيّر.
2. `hand نسبةً للجسم` يعود إلى **−8.30 بالضبط** عند الإطار الأخير = قيمته في الإطار 0 —
   فالدورة تُغلق في مكانها، ولا قفزة عند الالتفاف.

ومع `Force Root Lock = True` يُثبَّت الجذر الآن فعلاً ⇒ الجسم يبقى فوق الكبسولة.

### السرعة — لم تحتج تغييراً، والتحقّق يُثبته

```
PlayRate = 0.89  ⇒  مدّة الكليب = 0.667 / 0.89 = 0.749 ث
سفر الجذر المُؤلَّف = 67.37 سم
السرعة الفعليّة   = 67.37 / 0.749 = 89.9 سم/ث   ≈   MaxSpeed = 90   ✔
```

فالمعادلة `PlayRate = MaxSpeed / 101.1` كانت صحيحة، والعطب الوحيد كان الانزياح.

### كيف تتحقّق

```
افتح L2H_MD_Shimmy_L ▸ نافذة العرض
   شغّل الكليب: يجب أن يبقى الممثّل في مكانه والأطراف تتحرّك
   (لأن Force Root Lock مؤشَّر)
   وبإلغاء التأشير يجب أن يسافر 67 سم
```

على القرص: الكليبات الستّة بتاريخ `19:54:50` · مجموع `ATS/` = **14** كليباً.

### التراجع

أعِد استهداف الكليبات الستّة من `RTG_ALS_to_UEFN` فوقها (`overwrite = true`).
الأصول في `AdvancedTraversalSystem/` لم تُمسّ.

### درس

`force_root_lock` لا يُنتج تحذيراً حين لا يكون هناك جذر متحرّك ليقفله — يفشل صامتاً.
**بعد أي إعادة استهداف: اقرأ مسار `root` نفسه، لا مجرد وضعيّة العظام.**

---

## ١٣ — مونتاج الإمساك والجدولان

**تاريخ: 2026-09-24.**

### العائق الذي كُشف أوّلاً

`L2H_MD_grab_from_air` لا يصلح مونتاجاً كما هو:

| | `M_L2H_LedgeCatch_A` (العامل) | `L2H_MD_grab_from_air` |
|---|---|---|
| `enable_root_motion` | **True** | False |
| `force_root_lock` | False | **True** |
| مسار الجذر | `(0,−44.9,240.6) → (0,−42.7,124.0)` | **مسطّح عند صفر** |

**الالتفاف يطوي حركة الجذر.** وبلا حركة جذر لا يوجد ما يُطوى — الشخصيّة تلعب الأنيميشن
مكانها ولا تصل الحافة أبداً. نفس عطب القسم ١٢، ولكن بأثر أخطر.

### إعادة مسار الجذر من المصدر

على ثمانية كليبات حركة، نُسخ مسار `root` من أصل ATS وطُرح من الحوض:

```
root(f)   = sourceRoot(f)
pelvis(f) = currentPelvis(f) − sourceRoot(f)
enable_root_motion = True ,  force_root_lock = False
```

| الكليب | إزاحة الجذر (جانب، أمام، فوق) |
|---|---|
| `L2H_MD_grab_from_air` | `(0.00, +69.25, +29.84)` |
| `L2H_MD_Bar_Grab` | `(0.00, +67.00, 0.00)` |
| `L2H_MD_release` | `(0.00, −69.25, −25.93)` |
| `L2H_MD_bar_release` | `(0.00, −33.25, 0.00)` |
| `L2H_MD_corner_inner_l` / `_r` | `(±20.00, −20.00, 0.00)` |
| `L2H_MD_corner_outer_l` / `_r` | `(±60.00, +60.00, 0.00)` |

أرقام متّسقة مع معناها: الإمساك يتقدّم ويرتفع، والإفلات يتراجع وينزل، والزوايا تلتفّ.

### توقيت الإمساك — مقيس من الكليب

قِيست يد `L2H_MD_grab_from_air` (60 لقطة/ث):

```
t=0.000   fwd −47.0  up 117.6     ← بداية المدّ
t=0.200   fwd   0.5  up 174.6     ← أقصى امتداد
t=0.333   fwd  21.2  up 151.9     ← التماس
t=0.400   fwd  21.8  up 151.2     ← الاستقرار
t≥0.467   fwd  20.8  up 152.6     ← ثابت
```

والقيمة الثابتة `20.8 / 152.6` **مطابقة لوضعيّة `L2H_MD_hang_idle` (20.90 / 152.51)** —
فالمزج من المونتاج إلى حالة التعلّق بلا قفزة.

إذن: نافذة الالتفاف `0 → 0.40`، وإعادة الضبط عند `0.45`.

### المونتاجات السبعة

| المونتاج | الكليب | الطول | نافذة الالتفاف | إعادة الضبط | `ActivateState` |
|---|---|---|---|---|---|
| `AM_L2H_LedgeCatch` | `grab_from_air` | 1.333 | 0 → 0.40 | 0.45 | True |
| `AM_L2H_LedgeCatch_Bar` | `Bar_Grab` | 1.000 | 0 → 0.40 | 0.45 | True |
| `AM_L2H_LedgeRelease` | `release` | 0.667 | — | 0.45 | **False** |
| `AM_L2H_Corner_Inner_L` / `_R` | `corner_inner_*` | 0.967 | 0 → 0.72 | 0.82 | True |
| `AM_L2H_Corner_Outer_L` / `_R` | `corner_outer_*` | 1.600 | 0 → 1.20 | 1.36 | True |

على كلٍّ: `bEnableAutoBlendOut = False` · `BlendTime = 0.2` ·
`WarpTargetName = "height"` · **`bIgnoreZAxis = False`** · `UseStateWarpOverrides? = True`.

> **`ActivateState = False` على الإفلات وحده** — فهو الوحيد الذي يُخرج اللاعب من الحالة
> بدل أن يُبقيه فيها.

### الجدولان

**`DPTS_LedgeCatch_SelectionTable`** — صفّان الآن بدل واحد:

```
LedgeCatch      Target (LedgeClimb, FullContact)  →  AM_L2H_LedgeCatch
LedgeCatchHang  Target (LedgeClimb, Hang)         →  AM_L2H_LedgeCatch_Bar
```

**`DPTS_ClimbTransition_SelectionTable`** — كان فارغاً، صار خمسة صفوف:

| `PrimaryModification` | المونتاج |
|---|---|
| `CornerInnerRight` | `AM_L2H_Corner_Inner_R` |
| `CornerInnerLeft` | `AM_L2H_Corner_Inner_L` |
| `CornerOuterRight` | `AM_L2H_Corner_Outer_R` |
| `CornerOuterLeft` | `AM_L2H_Corner_Outer_L` |
| **`DisengageFromState`** | `AM_L2H_LedgeRelease` |

> **اكتشاف:** `DropOffAction` ليس جدول الإفلات من التعلّق. قراءة `E_CT_ModListing` أظهرت
> أن `DisengageFromState` هو العنصر **5** داخل `ClimbTransition`. و`DropOffAction` يقع تحت
> `FromGroundState` — أي النزول عن حافة **وأنت واقف على الأرض**، وهو فعل مختلف يحتاج كليباً
> آخر. **لم يُملأ** ومسجَّل كعمل متبقٍّ.

### على الشخصيّة

```
AllowedActionListing   ClimbTransition : False → True
IA_Jump ▸ Disengage from State ▸ UseAnimation?  : false → true
```

الثاني هو ما يجعل القفز أثناء التعلّق **يُشغّل مونتاج الإفلات** بدل الخروج الفوري.

### فخّ في كتابة الجداول — تكرّر وسُجّل

الكتابة الأولى فشلت بصمت جزئي:

```
"ok": false, "reason": "expected JSON array"
```

حقل `ActionSelectionValues` **مصفوفة**، فيجب تمريره **مصفوفة JSON من نصوص العناصر**، لا
سلسلة واحدة تحوي كل العناصر. الأداة أبلغت `modified: true` و`saved: true` **مع `errors: 1`**
— فلولا قراءة `field_writes` لظننتُها نجحت.

### كيف تتحقّق

```
افتح AM_L2H_LedgeCatch
   مُشعِر AN_MotionWarp_WithOffset  من 0 إلى 0.40 ▸ Ignore Z Axis غير مؤشَّر
   مُشعِر AN_ResetParkourAction عند 0.45 ▸ Activate State مؤشَّر
DPTS_ClimbTransition_SelectionTable ▸ صفّ L2H ▸ خمسة عناصر
BP_L2H_Base ▸ DPTS ▸ Allowed Action Listing ▸ Climb Transition مؤشَّر
```

قُرئت الجداول عكسيّاً بعد الحفظ (`export_data_table_to_json_string`) وأظهرت الخمسة
بأسمائها. على القرص: 7 مونتاجات · 16 كليباً في `ATS/` · `BP_L2H_Base` = 1,509,317 بايت.

### ما لم يُنفَّذ

| | لماذا |
|---|---|
| `DropOffAction` | فعل مختلف (من الأرض إلى التعلّق) — يحتاج كليب نزول، غير مُستهدَف بعد |
| `ClimbStart` · `ClimbJump` · `WallrunJump` | خارج نطاق الطلب |
| اختبار الزوايا | هندسة المستوى الحالي بلا زوايا — `LEDGE_01/02` كتلتان مستقيمتان |
| **لم أجرّب اللعبة** | كل ما سبق متحقَّق بنيويّاً وبالقياس والقراءة العكسيّة، لا سلوكيّاً |

---

## ١٤ — الإفلات: عطب صنعتُه، وخطأ تصميمي كشفه المستخدم

**تاريخ: 2026-09-24.** المستخدم: «الإفلات لا يعمل» + «لماذا جعلتَ الإفلات بالقفز؟ أليس
الإفلات بالجلوس في معظم الألعاب، والقفز للقفز أو الصعود؟»

### أ — لماذا توقّف الإفلات: أنا كسرتُه

في القسم ١٣ غيّرتُ `DisengageFromState ▸ UseAnimation?` من `false` إلى `true` لتشغيل
المونتاج الجديد. وتتبّع الدالّة يُظهر أن هذا يغيّر المسار كلّيّاً:

```
DisengageFromState( UseAnimation? )
   ├─ false →  End State Overrides
   │              Set ActiveParkourState=None · Set SubState · Plane Constraint
   │              · Base Movement Overrides · Clear targets · StateResetExecution
   │              ⇒ الإفلات يقع فوراً  ✔
   └─ true  →  Calculate Disengage Drop
                  … → Execute Climb Transition Action  (رسالة DPTS_BPI إلى الشخصيّة)
                  ⇒ ولا أحد يستمع  ✘
```

فحص واجهات `BP_L2H_Base`:

```
DPTS_BPI ينصّ على 11 دالّة، منها:
   Execute_VaultMantleAction        ← منفَّذة ✔
   Execute_LedgeCatchAction         ← منفَّذة ✔
   Execute_ClimbTransitionAction    ← غير منفَّذة ✘
```

**فالرسالة تُرسَل وتسقط في الفراغ، واللاعب يبقى معلّقاً بلا مخرج.**

وهذا يعني أيضاً أن **جدول `ClimbTransition` كلّه خامل** — الزوايا الأربع والإفلات — حتى
تُنفَّذ هذه الدالّة.

**أُعيد `UseAnimation? = false`** حتى يعمل الإفلات فوراً بلا أنيميشن.

> **الدرس:** الطلب الأصلي كان «شغّل مونتاج الإفلات». نفّذتُ الشرط الذي يُحوّل المسار **ولم
> أتتبّع المسار الجديد إلى نهايته** — نفس خطأ القسم ٦ حرفيّاً: قرأتُ من يُستدعى، ولم أسأل
> من يستمع.

### ب — الأزرار: المستخدم محقّ

الربط على القفز لم يكن قراراً تصميميّاً، بل بقيّة من جلسة سابقة لم يكن فيها أنيميشن إفلات
أصلاً. والعُرف في هذا النوع من الألعاب:

| | قبل | بعد |
|---|---|---|
| **الإفلات من الحافة** | `IA_Jump` | **`IA_Crouch ▸ Started`** |
| **`IA_Jump` أثناء التعلّق** | يُفلت | **حُرّر** — للصعود وقفز الحواف لاحقاً |

**ما نُفِّذ:**

```
IA_Crouch ▸ Started  →  Switch on S_ParkourStates  (جديد، يقرأ نفس ActiveParkourState)
                          1,2,3,4,6  →  Disengage from State ( UseAnimation? = false )

IA_Jump ▸ Switch ▸ 1,2,3,4,6  →  فُصلت    (المخرج 0 → SearchForParkourAction باقٍ كما هو)
```

استُعمل `Switch` جديد لا فرع بسيط، حتى **لا** يمرّ الجلوس على `NewEnumerator0` الذي يُشغّل
`SearchForParkourAction` — وإلّا لأصبح الجلوس على الأرض يُنفّذ قفزاً فوق العوائق.

> **أثر يجب أن تعرفه:** حتى تُبنى `ClimbJump`، **القفز أثناء التعلّق لا يفعل شيئاً**.
> والمخرج الوحيد هو الجلوس.

### كيف تتحقّق

```
BP_L2H_Base ▸ EventGraph ▸ IA_Crouch ▸ Started  موصول بـ Switch on S_ParkourStates
   مخارجه 1,2,3,4,6 → Disengage from State ▸ Use Animation غير مؤشَّر
IA_Jump ▸ Switch  مخارجه 1,2,3,4,6  فارغة
```

مقروء عكسيّاً بعد الترجمة: `Disengage.execute` يستقبل خمس وصلات، كلّها من `SwitchEnum_1`
(الجلوس)، ولا واحدة من `SwitchEnum_0` (القفز). ترجمة نظيفة.

### ج — ما لا تستطيع أدواتي فعله

عقدة تنفيذ حدث واجهة (`K2Node_Event` بمرجع إلى `DPTS_BPI`) **لا يمكن إنشاؤها برمجيّاً** هنا:

| ما جُرّب | النتيجة |
|---|---|
| `add_event_node` | أنشأ **حدثاً مخصّصاً** لا تنفيذ واجهة — حُذف |
| `override_parent_function` | أنشأ **رسم دالّة**، فرفضته الترجمة: `Graph named 'Execute_ClimbTransitionAction' already exists` — حُذف والترجمة عادت نظيفة |
| `add_node` · `build_blueprint_from_spec` | لا يدعمان نوع عقدة حدث الواجهة |

**الخطوة الوحيدة المتبقّية تحتاج المحرّر** — وهي نصف دقيقة:

```
1. افتح BP_L2H_Base ▸ EventGraph
2. في لوحة My Blueprint ▸ قسم Interfaces ▸ DPTS BPI
3. انقر يميناً على Execute_ClimbTransitionAction ▸ Implement Event
4. من مخرج الحدث اسحب: Anim Selection Climb Transition
      Target   ← Dynamic Parkour Trace System
      Priority ← Modification            ← وليس StateTypeMatch
5. ترجم واحفظ، ثم أعِد Disengage ▸ Use Animation إلى ✔
```

> **لماذا `Modification` لا `StateTypeMatch`:** صفوف `ClimbTransition` تتمايز بـ
> `PrimaryModification` (`CornerInnerRight` … `DisengageFromState`)، لا بالحالة.
> و`E_SelectionPriorityClimb` = 0 CompareValue · **1 Modification** · 2 StateTypeMatch.
> (حدث الإمساك يستعمل `StateTypeMatch` لأن صفوفه تتمايز بالحالة الفرعيّة.)

بعد هذه الخطوة تعمل **الزوايا الأربع ومونتاج الإفلات** دفعةً واحدة.

---

## ١٥ — استدعاء دائري في حدث الواجهة، وهندسة اختبار الزوايا

**تاريخ: 2026-09-24.** نفّذ المستخدم `Execute_ClimbTransitionAction` وعمل الإفلات، وسأل عن
معنى «الزوايا الأربع» وكيف يختبرها.

### أ — عطب في السلسلة التي وصّلها المستخدم

الصورة تُظهر:

```
Event Execute_ClimbTransitionAction → Anim Selection Climb Transition → Disengage from State
                                                                          Use Animation? = ✔
```

والعقدة الأخيرة **تُنشئ استدعاءً دائريّاً**:

```
DisengageFromState(true) → CalculateDisengageDrop → Execute_ClimbTransitionAction
      → AnimSelection_ClimbTransition → DisengageFromState(true) → …
```

الحدث نفسه هو ما يُطلقه `DisengageFromState(true)`، فمعاودة استدعائه من داخل معالجه تعود
إلى البداية. لم تتجمّد اللعبة لأن `CalculateDisengageDrop` يضع `Busy = true` فيفشل فرعه
الداخلي في الدورة الثانية ويذهب إلى `Reset Search` — نجاة بالصدفة لا بالتصميم.

**والأخطر:** الحدث نفسه يُطلَق أيضاً عند **الزوايا**. فمع بقاء هذه العقدة، كل محاولة التفاف
حول زاوية كانت ستنتهي بإفلات الحافة بدل الدوران.

**أُزيلت.** والنمط الصحيح مطابق لحدث الإمساك المجاور في نفس الرسم:

```
Event Execute_LedgeCatchAction        → Anim Selection Ledge Catch          (ولا شيء بعدها)
Event Execute_ClimbTransitionAction   → Anim Selection Climb Transition     (ولا شيء بعدها)
```

الإفلات الفعلي يقع من **داخل المونتاج**: `AM_L2H_LedgeRelease ▸ AN_ResetParkourAction`
عند 0.45 بـ`ActivateState = False`. لا حاجة لعقدة إضافية.

> `Priority = Modification` الذي ضبطه المستخدم **صحيح** — مقروء من العقدة: `NewEnumerator1`.

### ب — ما هي الزوايا الأربع

`E_CT_ModListing` يسمّيها بدلالة **اتجاه الزحف** و**نوع الانعطاف**:

| | الشكل | ما يحدث |
|---|---|---|
| **Inner** (داخليّة) | الجدار ينعطف **نحوك** — جيب مقعّر | تنطوي حول الركن فتصبح على الوجه المجاور |
| **Outer** (خارجيّة) | الجدار ينعطف **بعيداً** — طرف محدّب | تلتفّ حول الطرف إلى الوجه الخلفي |

وكلٌّ منهما `Left` و`Right` حسب اتجاه زحفك ⇒ أربع حالات.

وهذا يطابق إزاحات الجذر المقيسة في القسم ١٣:

```
corner_inner_*   (±20, −20, 0)    ← انطواء قصير إلى الداخل
corner_outer_*   (±60, +60, 0)    ← التفاف واسع إلى الخارج
```

### ج — هندسة الاختبار المضافة

شكل `L` واحد يعطي **الزاويتين معاً**:

```
CORNER_MAIN   x 6200→7000   y −100→ +100   Z 0→350   climb+ledge
CORNER_WING   x 6800→7000   y −900→ −100   Z 0→350   climb+ledge
CORNER_STEP   x 5900→6200   y −100→ +100   Z 0→175   بلا وسوم
```

مخطّط أفقي (المنظر من فوق · `+X` يميناً · `+Y` لأعلى):

```
                 y=+100  ┌──────────────────────┐
                         │      CORNER_MAIN     │
   ┌────────┐    y=−100  └───────────────┬──────┤
   │ STEP   │                            │ WING │
   └────────┘                            │      │
  x=5900  6200                           │      │
                                         └──────┘
                                    x=6800   7000       y=−900
```

**زاوية داخليّة:** تعلّق على وجه `y = −100` من `MAIN` وازحف نحو `+X`. عند `x = 6800` يعترضك
`WING`، فالحافة تنعطف نحوك ⇒ يجب أن تنطوي إلى وجه `x = 6800` من `WING`.

**زاوية خارجيّة:** تابع الزحف على `WING` نحو `−Y` حتى `y = −900` — الطرف المحدّب ⇒ تلتفّ
حوله إلى الوجه الخلفي.

ولافتات ثلاثيّة الأبعاد على الثلاثة.

### د — `AutoCorner?` فُعِّل

```
BP_L2H_Base ▸ DPTS ▸ Switches/ModificationValues ▸ AutoCorner?   False → True
```

بـ`False` يحتاج الالتفاف دفعاً باتجاه الزاوية؛ وبـ`True` يدور تلقائيّاً عند بلوغها —
**أوضح للاختبار الأوّل**. أعِدها `False` لاحقاً إن أردت التحكّم اليدوي.

### كيف تختبر

```
1. شغّل واذهب إلى x ≈ 5900  (شرق LEDGE_02)
2. اعتلِ CORNER_STEP ثم اقفز وأمسك CORNER_MAIN
3. ازحف يميناً (+X) حتى x = 6800      → يجب أن تدور زاوية داخليّة
4. تابع الزحف على WING حتى y = −900   → يجب أن تدور زاوية خارجيّة
5. الجلوس يُفلت في أي لحظة
```

**لو لم تدر:** شغّل تصحيح المجسّات (`DebugSettings ▸ EnableTraceDebug?` مفعّل أصلاً) وراقب
هل يجد النظام هدف الزاوية. و`CornerSearchStateMatch` على المكوّن يحوي أصلاً
`(LedgeClimb,FullContact)` و`(LedgeClimb,Hang)` و`(BalanceBeam,Hang)`، فالحالة مسموحة.

### كيف تتحقّق

```
BP_L2H_Base ▸ EventGraph ▸ Event Execute_ClimbTransitionAction
   → Anim Selection Climb Transition   (Priority = Modification)   ولا عقدة بعدها
World Outliner ▸ اكتب CORNER  ⇒ ستّة ممثّلين
```

ترجمة نظيفة. المستوى محفوظ.

### ما زال غير مختبَر

الزوايا نفسها — الهندسة جاهزة الآن لكن **لم تُجرَّب في اللعب**. وقد تحتاج
`SecondaryUseSettings ▸ CornerTargetDistance` (حاليّاً 30) ضبطاً حسب سماكة الجدار.

---

## ١٦ — الزوايا تعلق: صفّ فارغ يسبق صفّي في الجدول

**تاريخ: 2026-09-24.** المستخدم: الكرة الصفراء تظهر عند الزاوية، لكن اللاعب لا ينتقل
**ويعلق فلا يقدر على أي حركة**. وأحياناً على حافة مستقيمة، إن أمسكها بزاوية منحرفة قليلاً،
لا يستطيع التنقّل ويعلق.

### التشخيص

الكرة الصفراء = هدف `height` وُجد. فالبحث نجح، والحدث أُطلق. العطب بعده.

تتبّعتُ `AnimSelection_ClimbTransition`:

```
Get Data Table Row Names  →  Get Data Table Row [ RowUseIndex ]
    RowNotFound → Reset Search
    then → ForEach على ActionSelectionValues
              Validate Compare Values → Validate Climb Transition Mod → Add to list
           Completed → Branch ( القائمة فارغة؟ )
              then → Reset Search          ← لا مطابقة
              else → … → Trigger Action Montage
```

ثم قرأتُ أسماء صفوف الجدول:

```
DPTS_ClimbTransition_SelectionTable   rows = ['NewRow', 'L2H']
```

و`RowUseIndex = 0` على العقدة في الشخصيّة ⇒ **يقرأ `NewRow`**. وفحصُ محتواه:

```
ROW NewRow   elements=1   [0] mod=None   montages=[]      ← فارغ
ROW L2H      elements=5   القرنر الأربعة + Disengage      ← صفّي، عند الفهرس 1
```

**فصفوفي لم تُقرأ قطّ.** الجدول كان «فارغاً» في نظري، ولم أنتبه أنه يحوي صفّاً افتراضيّاً
فارغاً باسم `NewRow`، فأضفتُ صفّاً جديداً بدل ملء الموجود.

### لماذا يعلق بدل أن يفشل بهدوء

`Reset Search → Reset Temp Values` **يمسح `Busy`** (متحقَّق منه: `Set Busy = false`).
فالفشل بذاته لا يُعلّق. لكن مع `AutoCorner? = True` يتكرّر البحث **كل إطار** عند الزاوية:

```
يجد الهدف → Busy=true → يُطلق الحدث → لا مطابقة → ResetSearch → Busy=false → الإطار التالي…
```

حلقة فشل مستمرّة تمنع الزحف. وهذا يفسّر الأمرين: العلوق عند الزاوية، **وربّما** العلوق عند
الإمساك المنحرف — لأن الانحراف يجعل بحث الزاوية يُطلَق كذباً.

### الإصلاح

حُذف `NewRow`. والقراءة بعد الحفظ:

```
rows now: ['L2H']   →  RowUseIndex 0 يقرأ:  L2H
   [0] CornerInnerRight     AM_L2H_Corner_Inner_R
   [1] CornerInnerLeft      AM_L2H_Corner_Inner_L
   [2] CornerOuterRight     AM_L2H_Corner_Outer_R
   [3] CornerOuterLeft      AM_L2H_Corner_Outer_L
   [4] DisengageFromState   AM_L2H_LedgeRelease
```

### ملاحظة: لماذا كان الإفلات يعمل رغم ذلك

لأن الجلوس يستدعي `DisengageFromState( UseAnimation? = false )` — المسار المباشر
`EndStateOverrides`، الذي لا يمرّ بالجدول أصلاً. فالإفلات كان يقع **بلا أنيميشن**.
بعد إصلاح الصفّ يمكن إعادة `UseAnimation? = true` ليُشغَّل `AM_L2H_LedgeRelease`.

### تصحيح مرئي أُضيف

```
DebugSettings ▸ SearchTypeSelection :  (FrontalBlockSearch)
                                    →  (FrontalBlockSearch, ClimbTransitionSearch, ClimbValidations)
DrawTime 5 → 2 ث   ·   DebugSphere Duration 10 → 5 ث
```

`ClimbValidations` هو المجسّات التي تقرّر **هل يُسمح بالزحف في هذا الاتجاه** — وهي أداة
تشخيص العطب الثاني (الإمساك المنحرف) مباشرةً.

### كيف تتحقّق

```
DPTS_ClimbTransition_SelectionTable  ⇒  صفّ واحد اسمه L2H
BP_L2H_Base ▸ DPTS ▸ Debug Settings ▸ Search Type Selection  ⇒  ثلاثة عناصر
```

على القرص: الجدول `7,655` بايت · `BP_L2H_Base` = `1,522,897` بايت.

### الدرس

«الجدول فارغ» ليس نفس «الجدول بلا صفوف». `DPTS_ClimbTransition_SelectionTable` كان يحوي
`NewRow` فارغاً، و`RowUseIndex` يقرأ **بالترتيب لا بالاسم**.

> **القاعدة:** قبل إضافة صفّ إلى جدول اختيار، اقرأ `get_data_table_row_names` أوّلاً
> واملأ الصفّ عند الفهرس الذي تستدعيه العقدة — أو احذف ما قبله.

---

## ١٧ — شرط الزاوية الحقيقي: `ClimbValidationFailIndex` ودخل الحركة

**تاريخ: 2026-09-24.** المستخدم بعد إصلاح الصفّ: المجسّات تُرسم والهدف يُوجد، ولا انتقال.

### الشرط — مقروء من `Validate_CT_Mod_Single`

الدالّة `Select` على `PrimaryModification`:

```
Option 0  None              =  ValidateNone?
Option 1  CornerInnerRight  =  ( ClimbValidationFailIndex == 1 )  AND  ( Right Input >  0 )
Option 2  CornerInnerLeft   =  ( ClimbValidationFailIndex == 1 )  AND  ( Right Input <  0 )
Option 3  CornerOuterRight  =  ( ClimbValidationFailIndex == 0 )  AND  ( Right Input >  0 )
Option 4  CornerOuterLeft   =  ( ClimbValidationFailIndex == 0 )  AND  ( Right Input <  0 )
Option 5  DisengageFromState=  IsDrop?
```

(مقابيس `B` على `Equal (Integer)` للخيارَين 3 و4 غير موصولة بلا قيمة ⇒ **0**؛ وللخيارَين 1 و2
قيمتها الافتراضيّة **1**.)

**نتيجتان لم تكونا معروفتين:**

1. **الزاوية لا تعمل إلا واللاعب ضاغط على اتجاه الحركة.** `Right Input` صفر ⇒ لا خيار يصلح.
   فلو وصلتَ الزاوية ثم رفعت إصبعك، لا شيء يحدث.
2. **نوع الزاوية لا يُقرّره شكل الجدار، بل رقم المجسّ الذي فشل:**
   `0` ⇒ خارجيّة · `1` ⇒ داخليّة.

### ولماذا كان `AutoCorner? = True` يمنعها

```
UpdateParkourStateData ▸ Branch ( AutoCorner? )
   then  →  Calculate Corner ( FailIndex )          ← مسار تلقائي
   else  →  Set ClimbValidationFailIndex = FailIndex ← المسار الذي يقرأه Validate_CT_Mod_Single
```

المتغيّر العضو **لا يُكتب إلا في الفرع `else`**. فبـ`True` يبقى على `−1` (قيمة
`ResetTempValues`)، **ولا يساوي 0 ولا 1 أبداً** ⇒ لا صفّ زاوية يصلح ⇒ فشل كل إطار ⇒ العلوق.

**وأنا من فعّل `AutoCorner?` في القسم ١٥** ظنّاً أنه يُسهّل الاختبار. أُعيد إلى `False`.

> **نفس الخطأ للمرّة الثالثة:** قلبتُ مفتاحاً لأُسهّل شيئاً **دون قراءة ما يفعله المفتاح**.
> القسم ٦ (بوّابة الصفر) والقسم ١٤ (`UseAnimation`) والآن هذا.

### توسيع مطابقة الحالة

`Validate_StateTypeMatch` يُستدعى بـ`TargetOnly? = false` ⇒ يفحص **`Target` و`Active` معاً**.
وصفوفي كانت تملأ `Active` فقط و`Target` فارغاً. مُلئ الحقلان على الصفوف الخمسة بـ:

```
(LedgeClimb, FullContact)  و  (LedgeClimb, Hang)
```

قراءة عكسيّة بعد الحفظ: `Target=2 Active=2` على الخمسة.

### ما غُيِّر

| | قبل | بعد |
|---|---|---|
| `AutoCorner?` | True | **False** |
| صفوف الزوايا ▸ `StateMatchRestriction.Target` | فارغ | **حالتان** |
| صفوف الزوايا ▸ `StateMatchRestriction.Active` | حالة واحدة | **حالتان** |

### كيف تختبر — والشرط الحاسم

```
1. أمسك CORNER_MAIN
2. ازحف نحو +X  و **أبقِ الإصبع مضغوطاً** حتى بعد توقّف الحركة
3. عند الركن يفشل التحقّق ⇒ يُسجَّل الفهرس ⇒ تُنتقى الزاوية
```

**رفع الإصبع يُلغي الشرط.** وهذا هو الفرق بين «عالق» و«لم أضغط بما يكفي».

### لو بقي لا يدور

المتغيّران المطلوب رؤيتهما أثناء العلوق، على المكوّن في الـOutliner:

```
ClimbValidationFailIndex     يجب أن يكون 0 أو 1  (لا −1)
Right Input                  يجب ألّا يكون صفراً
```

- `−1` ⇒ التحقّق لم يفشل أصلاً، أي أن المجسّات تجد مساحة صالحة ⇒ الحافة غير متّصلة كما توقّعت،
  أو `ValidationOffset` (40) أقصر من أن يبلغ الركن.
- `2` أو أكثر ⇒ فشل مجسّ ثالث لا تغطّيه جداول DPTS الأصليّة.

على القرص: الجدول محفوظ · `BP_L2H_Base` محفوظ · ترجمة نظيفة.

---

## ١٨ — من يستدعي بحث الزاوية: زرّ البحث، لا التلقائي

**تاريخ: 2026-09-24.** المستخدم بعد إطفاء `AutoCorner`: «لم تعمل، حتى الكرة الصفراء لم تعد
تظهر».

### الدلالة

اختفاء الكرة هو المفتاح: الكرة تُرسم عند **إيجاد هدف**. فبـ`AutoCorner = True` كان
`Calculate Corner` يعمل تلقائيّاً كل إطار فيجد الهدف ويرسمه — لكن دون أن يكتب
`ClimbValidationFailIndex` فلا ينتقي صفّاً. وبـ`False` لا يعود يبحث أصلاً، فيختفي الهدف.

**أي أن الوضعين كلاهما كان معطّلاً، لسببين مختلفين.**

### المسار الصحيح

تتبّعتُ `FindClimbTransitionTarget` إلى مستدعيه الوحيد في `EventGraph` المكوّن:

```
RPCMC_SearchForParkourAction          ← يُطلقه SearchForParkourAction
   → Branch ( ActiveParkourState == None )
        then →  بحث الأرض   (Vault / Mantle)
        else →  Switch ( InStateActionSearchPriority )
                   0 → Find Climb Jump Target → (فشل) → Find Climb Transition Target
                   1 → Find Climb Transition Target
```

**فبحث الزاوية يُشغَّل بالضغط على زرّ البحث أثناء التعلّق.** وهو نفس الزرّ الذي يبحث عن
الاعتلاء وأنت على الأرض — الفرع الأوّل للحالة `None`.

وفي القسم ١٤ فصلتُ `IA_Jump` عن حالات التسلّق كلّها، فلم يبقَ من يستدعي البحث.

### ما غُيِّر

```
IA_Jump ▸ Switch on S_ParkourStates
     0                →  Search for Parkour Action      (كما كان — اعتلاء من الأرض)
     1,2,3,4,6        →  Search for Parkour Action      ← أُعيد التوصيل، لكن إلى البحث
                                                          لا إلى Disengage كما كان أصلاً
IA_Crouch ▸ Switch ▸ 1,2,3,4,6  →  Disengage from State  (كما ضُبط في القسم ١٤)

InStateActionSearchPriority :  NewEnumerator0  →  NewEnumerator1
```

الأولويّة `1` تذهب إلى بحث الانتقال **مباشرةً**. الأصل `0` يجرّب قفز التسلّق أوّلاً ويسقط
إلى الانتقال عند فشله — وهو الصحيح يوم يُبنى `ClimbJump`، لكنه اليوم يمرّ بجدول فارغ بلا داعٍ.

> **وهذا يُثبت أن ملاحظة المستخدم عن الأزرار كانت أدقّ ممّا فهمتُ وقتها.** ظننتُه يقول
> «حرّر القفز»، والتصميم الفعلي هو:
>
> ```
> القفز   =  ابحث وتقدّم   (اعتلاء · قفز بين الحواف · دوران زاوية)
> الجلوس  =  أفلت
> ```
>
> أي أن القفز لم يكن يجب أن يُترك فارغاً، بل أن يُوجَّه إلى البحث.

### كيف تتحقّق

```
BP_L2H_Base ▸ EventGraph ▸ IA_Jump ▸ Switch  ⇒  المخارج الستّة كلّها إلى
                                                Search for Parkour Action
BP_L2H_Base ▸ DPTS ▸ In State Action Search Priority  =  الخيار الثاني
```

مقروء عكسيّاً بعد الترجمة: المخارج `0,1,2,3,4,6` جميعها موصولة بعقدة البحث نفسها.

### كيف تختبر الزوايا الآن

```
1. أمسك CORNER_MAIN
2. ازحف نحو الركن و **أبقِ الإصبع على اتجاه الحركة**
3. **اضغط القفز**        ← هذا ما يُشغّل البحث
```

الشرطان معاً مطلوبان: `Right Input ≠ 0` من القسم ١٧، **والقفز** من هنا.

---

## ١٩ — الوثيقة الرسميّة (P5 Climb Transitions) تُصحّح ثلاثة مواضع

**تاريخ: 2026-09-25.** قدّم المستخدم تفريغ درس المؤلّف الرسمي. قارنتُه بما بنيتُه.

### ما أكّدته الوثيقة

| | |
|---|---|
| `AutoCorner` مطفأ ⇒ الحركة تتطلّب **اتجاه + قفز** | يطابق استنتاجي في القسمين ١٧ و١٨ حرفيّاً |
| `Corner Search State Match` على المكوّن | كان مضبوطاً أصلاً |
| القفز للبحث والتقدّم | يطابق القسم ١٨ |

### ما صحّحته — ١: مونتاج الزاوية يحتاج مُشعِرَي التفاف لا واحداً

بنيتُ مُشعِراً واحداً نحو `height`. والوثيقة تنصّ على مرحلتين:

```
① Warp → "initial"      يمسك الزاوية أوّلاً
     Use State Warp Overrides = ✘
     Primary Offsets : Forward · Up · Right/Left
② Warp → "height"       يدور نحو السطح الجديد
     Orientation Use = PrimaryTargetNormal
     Use State Warp Overrides = ✔
```

بمُشعِر واحد تقفز الشخصيّة إلى السطح الجديد بلا إمساك الركن — وهو ما لم يكن ليبدو صحيحاً حتى
لو عمل.

**أُعيد بناء المونتاجات الأربعة** بالمرحلتين.

### ما صحّحته — ٢: التوقيت كان أطول من الحركة

قِست حركة الجذر في الكليبات:

```
corner_outer_l   root → (60, 60, 0)   ويثبت عند t = 0.533   (الكليب 1.600)
corner_inner_l   root → (20, −20, 0)  ويثبت عند t = 0.633   (الكليب 0.967)
```

فنوافذي (0→1.20 و0→0.72) كانت تمتدّ بعد انتهاء الحركة. التوقيت الجديد:

| | نافذة `initial` | نافذة `height` | إعادة الضبط |
|---|---|---|---|
| داخليّة | 0 → 0.30 | 0.30 → 0.65 | 0.70 |
| خارجيّة | 0 → 0.25 | 0.25 → 0.55 | 0.60 |

### الإزاحات — مقيسة من أنيميشننا لا منقولة

الوثيقة تعطي `Forward = −50` و`Up = −120` لكليبات المؤلّف. وقياسي لكليباتنا:

```
corner_outer_l  f0 :  hand_r fwd = 20.90  up = 152.51
corner_inner_l  f0 :  hand_r fwd = 20.90  up = 152.51
L2H_MD_hang_idle f0:  hand_r fwd = 20.90  up = 152.51      ← مطابق تماماً
```

**الكليبات الثلاثة تبدأ بوضعيّة التعلّق نفسها بالضبط.** فإزاحة `initial` هي أرقامنا المضبوطة:

```
ForwardOffset = −20.9      UpOffset = −152.5
RightOffset   = −40 (يسار)  ·  +40 (يمين)     ← رقم المؤلّف، لا أستطيع اشتقاقه من الوضعيّة
```

> الإزاحة الجانبيّة وحدها منقولة عن الوثيقة، لأنها قيمة تصميم لا تُقاس من عظم.

### ما صحّحته — ٣: مونتاج الإفلات كان ناقص مُشعِر

الوثيقة: الإفلات **لا يحتاج التفافاً**، بل يحتاج `Reset DPTS Movement Overrides`:

```
Movement Mode      = Falling
Include Constraint = True        ← يرفع Plane Constraint المفروض أثناء التسلّق
Gravity Override   = 1.75
```

مونتاجي كان يحوي `Reset Parkour Action` فقط. **وبلا رفع قيد المستوى قد لا يسقط اللاعب أصلاً.**
أُضيف المُشعِر عند `t = 0.15`.

### وقيد الاتجاه على الصفوف

الوثيقة تنصّ على `Input Restriction = Left / Right` لصفوف الزوايا، وتركتُها `None`:

```
CornerInnerRight · CornerOuterRight  →  Right   (E_DirectionSelection = 3)
CornerInnerLeft  · CornerOuterLeft   →  Left    (= 4)
DisengageFromState                   →  None
```

### فخّ وقع أثناء التنفيذ — والقراءة العكسيّة أمسكته

ضبطتُ قيد الاتجاه عبر تصدير JSON ← تحرير ← استيراد. والقراءة بعدها أظهرت:

```
[0] CornerInnerRight  input=Right   montages=['None']     ← المونتاجات ضاعت
```

لأنني كنتُ قد **حذفتُ المونتاجات الأربعة وأعدتُ بناءها**، فصارت مراجع JSON القديمة معلّقة
وكُتبت `None`. أُعيدت الصفوف الخمسة كاملةً بـ`set_data_table_rows`.

> **لولا القراءة العكسيّة لكان الجدول يبدو مضبوطاً وهو بلا مونتاج واحد.**

### التحقّق النهائي — مقروء من المحرّك

```
[0] CornerInnerRight  input=Right  T=2 A=2  AM_L2H_Corner_Inner_R
[1] CornerInnerLeft   input=Left   T=2 A=2  AM_L2H_Corner_Inner_L
[2] CornerOuterRight  input=Right  T=2 A=2  AM_L2H_Corner_Outer_R
[3] CornerOuterLeft   input=Left   T=2 A=2  AM_L2H_Corner_Outer_L
[4] DisengageFromState input=None  T=2 A=2  AM_L2H_LedgeRelease

Corner_*   warp[initial ovr=False Z=False]  warp[height ovr=True Z=False]  reset[Act=True]
LedgeRelease                                reset[Act=False]  move[Falling constr=True g=1.75]
LedgeCatch warp[height ovr=True Z=False]    reset[Act=True]
```

### ما تبقّى من الوثيقة ولم يُنفَّذ

| | لماذا |
|---|---|
| `Safety Vault` · `Monkey Climb Up` | تحتاج مُشعِر `Find Land Target` وكليبات اعتلاء من التعلّق — غير مُستهدَفة |
| `Beam Walk ↔ Hang` | حالة `BalanceBeam` غير مبنيّة |
| `180 Turns` · `Ledge Turn` | تحتاج `Custom Bool` ومفتاحاً إضافيّاً |
| `Instate Action Search Priority` على مفتاح | ضُبط ثابتاً على `Transition` في القسم ١٨؛ الوثيقة تجعله مفتاحاً (Ctrl) — يوم يُبنى `ClimbJump` |
| `Compare Values` = `Length` | صفوف الزوايا لا تحتاجها؛ تلزم للـVault/Mantle |

---

## ٢٠ — لماذا لا يُرى أنيميشن الزاوية: المقبس في الفرع المتجاوَز

**تاريخ: 2026-09-25.** المستخدم: «الزوايا تعمل — بقي الأنيميشن الخاص بها».

### التشخيص — لا ينقص أنيميشن

تتبّعتُ وصلات `AnimGraph`:

```
Motion Matching → … → Aim Offset → Slot 'DefaultSlot' → Offset Root Bone
    → Foot Placement → Leg IK → Pose History ──→ BlendListByBool_1.BlendPose_1
ClimbStates ─────────────────────────────────→ BlendListByBool_1.BlendPose_0
                          bActiveValue ◂── IsClimbing        → Output Pose
```

**المقبس الوحيد في المشروع يقع داخل `BlendPose_1`** — الفرع الذي نتجاوزه كلّما كان
`IsClimbing = true`. فالمونتاج يُشغَّل فعلاً وDPTS ينقل الشخصيّة (ولهذا «تعمل» الزوايا)،
لكن **وضعيّته لا تصل المخرج أبداً**.

وهذا يفسّر لماذا يُرى مونتاج الإمساك ولا يُرى مونتاج الزاوية:

| | الحالة لحظة التشغيل | `IsClimbing` | المقبس |
|---|---|---|---|
| الإمساك | `None` حتى يُفعّلها `AN_ResetParkourAction` | **false** | مرئي ✔ |
| الزاوية · الإفلات | `LedgeClimb` مسبقاً | **true** | متجاوَز ✘ |

### الإصلاح

أُضيف مقبس ثانٍ داخل فرع التسلّق:

```
ClimbStates → Slot 'DefaultSlot' → BlendListByBool_1.BlendPose_0
```

**بالاسم نفسه عمداً.** جرّبتُ أوّلاً اسماً منفصلاً `ClimbSlot`، لكن تسجيل مقبس جديد على
الهيكل غير متاح برمجيّاً:

```
add_slot_node → "Slot 'ClimbSlot' is not registered on skeleton 'SK_UEFN_Mannequin'.
                 Known slots: [DefaultSlot, UpperBody]"
Skeleton::RegisterSlotNodeName → غير مكشوفة لـPython
```

واستعمال `DefaultSlot` مرّتين آمن هنا لأن **الفرعين لا ينشطان معاً أبداً** — المزج بوليّاني.
والترجمة أكّدت: **0 أخطاء، 0 تحذيرات**، فالمحرّك لا يعترض على التكرار.

### كيف تتحقّق

```
ABP_L2H_Base ▸ AnimGraph
   ClimbStates ──▸ Slot 'DefaultSlot' ──▸ Blend Poses by bool ▸ BlendPose_0
```

مقروء عكسيّاً: `AnimGraphNode_Slot_1.Source ◂── StateMachine_1.Pose` و
`Slot_1.Pose ──▸ BlendListByBool_1.BlendPose_0`. وكل المونتاجات الستّة على `DefaultSlot`.

على القرص: `ABP_L2H_Base` محفوظ · ترجمة نظيفة.

### التراجع

احذف `AnimGraphNode_Slot_1` وأعِد وصل `ClimbStates` مباشرةً بـ`BlendPose_0`.

### أثر جانبي متوقَّع

مونتاجات الزاوية والإفلات ستُرى الآن **فوق** وضعيّة `ClimbStates`. وأثناء الإفلات يُطفئ
`AN_ResetParkourAction` الحالة عند `t = 0.45`، فيقفز الرسم إلى فرع GASP ويصبح ما تبقّى من
المونتاج (0.22 ث) مرئيّاً عبر المقبس القديم — أي لا انقطاع، لأن المقبسين يحملان المونتاج نفسه.

---

## ٢١ · الصعود من التعلّق + علاج حسّاسيّة الاتجاه

### ما المشكلة التي حُلَّت

١. **لا مخرج للأعلى من الحافة.** كنت تُمسك، تزحف، تلتفّ — ثم إمّا تُفلت أو تعلق.
٢. **الكشف حسّاس للزاوية.** سبب مقروء من الكود، لا تخمين (انظر أدناه).

### أ · لماذا كان الكشف حسّاساً

`DPTS_BFL ▸ GetDirectionFromInput` يقسّم الإدخال إلى ثماني جهات، وكل مقارناته
مربوطة بـ **صفر مضبوط**:

| الشرط | الناتج |
|---|---|
| `F > 0` و `R == 0` | Forward |
| `F == 0` و `R > 0` | Right |
| `F > 0` و `R > 0` | ForwardRight |

ثم `Validate_InputDirection` — حين يكون `InputRestriction` غير `None` — يطلب تطابقاً
تامّاً مع هذه الجهة.

وكانت صفوف الزوايا مضبوطة على `Right`/`Left`، أي أنّها تطلب **`ForwardInput == 0.0`
بالضبط**. على عصا التحكّم هذا لا يحدث أبداً: دفعة يميناً تعطي `X≈0.98, Y≈0.05`
فتُقرأ `ForwardRight` ولا تطابق `Right` أبداً.

والأسوأ أنّ هذا الشرط **زائد**: `Validate_CT_Mod_Single` يفحص أصلاً إشارة
`RightInput` (`> 0` لليمين، `< 0` لليسار) لكل صفّ زاوية.

### ب · العلاج — منطقة ميتة ثُمُنيّة في مخطّطنا نحن

المكان: `BP_L2H_Base ▸ EventGraph`، على مخرج `IA_Move` **قبل**
`ParkourInputOverride` — لم يُمسّ أيّ أصل من DPTS أو Epic.

```
IA_Move ▸ ActionValue_X ─┬─→ Abs ─→ ( > 0.414214 ) ─→ bPickA ┐
                         └────────────────────────→ A       ├─ SelectFloat → RightInput
                                                    B = 0.0 ┘
            ActionValue_Y ─┬─→ Abs ─→ ( > 0.414214 ) ─→ bPickA ┐
                           └──────────────────────────→ A     ├─ SelectFloat → ForwardInput
                                                      B = 0.0 ┘
```

**من أين جاء 0.414214؟** الدائرة ثمانية قطاعات ⇒ كل قطاع 45° ⇒ حدّه ±22.5° عن المحور.
`tan(22.5°) = 0.414214`. فأيّ محور أصغر من 0.414 من الآخر يُصفَّر، وتصبح القطاعات
الثمانية متساوية تماماً. ليس رقماً مخترعاً — هو 45°÷2.

| الدفع | قبل | بعد |
|---|---|---|
| يميناً (X=1.00, Y=0.05) | ForwardRight ✘ | **Right** ✔ |
| 20° عن اليمين (0.94, 0.34) | ForwardRight ✘ | **Right** ✔ |
| قطريّاً 45° (0.71, 0.71) | ForwardRight ✔ | ForwardRight ✔ |
| لوحة المفاتيح D وحدها (1, 0) | Right ✔ | Right ✔ |

وأُزيل أيضاً القيد الزائد: صفوف الزوايا الأربعة صار `InputRestriction = None`،
فتبقى إشارة `RightInput` وحدها هي الحَكَم.

**للتراجع:** أعِد `[0],[2]` إلى `Right` و`[1],[3]` إلى `Left`، واحذف عُقَد
`Abs/>/SelectFloat` الستّ.

### ج · الصعود — من أين يأتي

ليس في DPTS فعلٌ اسمه «صعود». تتبّعت السلسلة:

- `CalculateClimbStart` يُستدعى من `FindGroundActionSelection` و`CalculateDropAction`
  فقط ⇒ **ClimbStart = دخول التسلّق من الأرض**، لا الخروج منه.
- `Validate_CT_Mod_Single ▸ Option 5 (DisengageFromState)` شرطه الوحيد `IsDrop?`.
- و`IsDrop?` لا يضبطه إلّا `CalculateDisengageDrop`، ولا يُستدعى إلّا من
  `DisengageFromState(UseAnimation = true)`.

⇒ **الصعود والإفلات المتحرّك يسلكان المسار نفسه**، ويفترقان بـ`InputRestriction`.

### د · الكليب والأرقام

`MD_climb_up_ALS` (ATS · 1.5 ث · 90 إطاراً · 60 fps) → `L2H_MD_climb_up`.

| القياس | القيمة | كيف |
|---|---|---|
| وضعيّة البداية `hand_r` | **(20.900 , 152.511)** | مطابقة تماماً لـ`L2H_MD_hang_idle` ⇒ يمتزج من التعلّق بلا قفزة |
| معامل القياس بين الهيكلين | **0.962466** | من نسبة انتقال الحوض: أفقيّاً 0.962466، رأسيّاً 0.962466 — متطابقان لستّ منازل |
| انتقال الجذر | **أماماً 44.255 · فوق 171.319** | جذر المصدر (45.981 , 178.000) × 0.962466 |

**عطب إعادة الاستهداف:** `RTG_ALS_to_UEFN ▸ Root Motion` كان `SourceRoot=TargetRoot=pelvis`
فخرج الجذر مسطّحاً. جرّبت ضبطه على `root`: النتيجة أسوأ — نسخ إزاحة ثابتة +20 سم
كأنّها حركة، وأنزل الجسم 3.63 سم فكسر معايرة 152.511. وجرّبت
`GENERATE_FROM_TARGET_PELVIS`: خطأ كذلك. **فأُعيد المُستهدِف كما كان**، ونُقل مسار
الجذر يدويّاً:

```
root(f)   = 0.962466 × جذر_المصدر(f)
pelvis(f) = pelvis_قديم(f) − root(f)
```

تحقّق: الحوض عند الإطار 90 بقي (0 , 60.834 , 267.282) و`hand_r` عند الإطار 0 بقي
(20.9007 , 152.5114) — **الوضعيّة العالميّة لم تتغيّر إطلاقاً**.

### هـ · المونتاج `AM_L2H_ClimbUp`

توقيتات مقروءة من مسار الجذر نفسه، لا مقدَّرة:

| الإطارات | الزمن | ماذا يجري |
|---|---|---|
| 0–22 | 0 – 0.383 | ثابت (الشدّ) |
| 23–52 | 0.383 – 0.867 | صعود |
| 52–70 | 0.867 – 1.167 | خطوة للأمام |
| 70–90 | 1.167 – 1.5 | استقرار |

| المُشعِر | الزمن | الضبط |
|---|---|---|
| `AN_FindLandTarget` | 0.30 | `height` · Up +20 · MaxZ 90 · أمام 30→90 |
| `AN_MotionWarp_WithOffset` | 0.383 → 1.167 | هدف `land` · `bIgnoreZAxis=false` · `bWarpToFeetLocation=true` |
| `AN_ResetParkourAction` | 1.20 | Blend 0.2 · `ActivateState=false` · `MOVE_Walking` |

أرقام `AN_FindLandTarget` ليست تخميناً: نُسخت من `AM_L2H_Mantle_1_0_run_F_Rfoot`
العامل في المشروع — نفس الهندسة (حافة أمامك، مهبط خلفها).

> **`LoopTrace` — كيف يعمل فعلاً:** يدير الترتيبة −90° بايتش، فيصير «أمامه» = أسفل
> و«فوقه» = أمام اللاعب. فالبحث يخطو **10 سم أماماً** لكل فهرس (`Scale.Z × 2 = 10`)
> ويتتبّع **نزولاً** بمقدار `MaxLandSeachZDistance`.

### و · الجدول والإدخال

`DPTS_ClimbTransition_SelectionTable ▸ L2H` صار ستّة عناصر:

| # | التعديل | الاتجاه | المونتاج |
|---|---|---|---|
| 0–3 | الزوايا الأربع | **None** *(كان Right/Left)* | `AM_L2H_Corner_*` |
| 4 | DisengageFromState | **Forward** | **`AM_L2H_ClimbUp`** |
| 5 | DisengageFromState | None | `AM_L2H_LedgeRelease` |

العنصران 4 و5 يتنافسان حين يُمسَك الأمام؛ و`Filter_InputRestriction` يرجّح
**صاحب القيد** على غير المقيَّد ⇒ يفوز الصعود. وبلا اتجاه يبقى 5 وحده.

أمّا الإدخال في `BP_L2H_Base`:

```
IA_Jump ▸ Started → Branch(NOT Busy) → Switch(S_ParkourStates)
    ├ 0 (لا حالة) ─────────────────→ SearchForParkourAction      (قفز/تسلّق/إمساك)
    └ 1,2,3,4,6 (متعلّق) → Branch(ParkourInputDirection == Forward)
                              ├ صحيح → DisengageFromState(UseAnimation = TRUE)   ← الصعود
                              └ خطأ  → SearchForParkourAction                    ← الزوايا
IA_Crouch ▸ Started → Switch → DisengageFromState(UseAnimation = false)           ← إفلات فوري
```

أُضيف متغيّر `ParkourInputDirection` (`E_DirectionSelection`) يُكتب عند `IA_Move`
من `GetDirectionFromInput` بالقيم بعد المنطقة الميتة — فيتّفق شرط المخطّط مع شرط
الجدول حرفاً بحرف، ولا يمكن أن يختارا عنصرين مختلفين.

الترجمة: **0 أخطاء، 0 تحذيرات.**

### ز · حدود معروفة

- `CalculateDisengageDrop` يتتبّع **350 سم نزولاً ويشترط إصابة**. فوق هاوية أعمق
  من ذلك لن يعمل الصعود (ولا الإفلات المتحرّك).
- كليبات `_long` و`bar` الثلاثة أُعيد استهدافها وحُفظت لكن **جذرها ما زال مسطّحاً**
  — لم يُنقل إلّا جذر `L2H_MD_climb_up`. لذلك يُشير الصفّ إلى المونتاج المشدود
  للحالتين (كما يفعل `ClimbStates` أصلاً).
- `batch_retarget_animations` بـ`overwrite=true` **يُسقِط المحرّر** (قراءة مؤشّر فارغ
  في `MonolithAnimationActions.cpp:11337`). احذف الأصول أوّلاً ثم أعِد الاستهداف.

### كيف تتحقّق

1. تعلّق بحافة، **امسك الأمام واضغط القفز** ⇒ يصعد ويقف.
2. امسك يميناً/يساراً واضغط القفز ⇒ زاوية كما قبل، لكن الآن مع انحراف حتى 22.5°.
3. اضغط الجلوس ⇒ إفلات فوري (لم يتغيّر).

---

## ٢٢ · لماذا لم يصعد — ومسار بديل

### العَرَض

«عند القفز باتجاه الجدار لا كرات ترسم ولا يصعد.»

### السبب — عثرة `Distance > 0`

المسار الذي بنيته في القسم ٢١ كان:

```
Jump + أمام → DisengageFromState(UseAnimation = true) → CalculateDisengageDrop
```

و`CalculateDisengageDrop` يتتبّع **نزولاً 350 سم بصندوق نصف‑حجمه 30** من ترتيبة
الممثّل، ثم يمرّ الناتج على `DPTS_BFL ▸ ValidateHitResult`:

```
Success? = bBlockingHit  AND  (Distance > 0)
```

وأنت متعلّق بالحافة: `StateWarpOverrides[0] ▸ ForwardOffset = −24.4` يضع الممثّل
24.4 سم خلف الشفة. فصندوق نصف‑حجمه **30** يمتدّ أماماً 30 سم ⇒ يخترق واجهة الجدار
5.6 سم عند الانطلاق ⇒ **تماسّ ابتدائي** ⇒ `Distance = 0` ⇒ `Success = false`.

والفرع الخاطئ في `CalculateDisengageDrop` **لا يقود إلى شيء** — لا حدث، ولا إعادة
ضبط، ولا رسم. ولأن تتبّعه يستعمل `TraceSettings` غير موصول (⇒ `DrawDebugType = None`)
**لا يُرسم شيء أصلاً**. فالعَرَض «لا كرات ولا صعود» هو بالضبط توقيع هذا الطريق المسدود.

> هذا يفسّر أيضاً لماذا لم يظهر العطب من قبل: الإفلات كان يمرّ بـ
> `UseAnimation = false` ⇒ `EndStateOverrides` مباشرةً، فلم يُستدعَ
> `CalculateDisengageDrop` ولا مرّة.

### العلاج — استعمال مُطلِق المونتاج نفسه الذي تنتهي إليه كل جداول DPTS

`TriggerActionMontage` دالّة **عامّة** على المكوّن، وهي آخر عقدة في
`AnimSelection_ClimbTransition` و`AnimSelection_LedgeCatch` وغيرهما. تستدعيها مباشرةً
فتتخطّى التتبّع النزولي كلّياً:

```
Branch(ParkourInputDirection == Forward) ▸ TRUE
    → Set DPTS.Busy = true
    → TriggerActionMontage( [AM_L2H_ClimbUp],
                            DisableConstraint?        = true,
                            IgnoreMovementOverrides?  = false,
                            IsTransition?             = false )
```

**لماذا `Busy = true`؟** لأن `ActiveClimbTransformAdjustment` — الدالّة التي تلصق
الشخصيّة بالحافة كل إطار — **تقرأ `Busy`**. بدونها ستشدّ الشخصيّة إلى الحافة وتقاوم
الـwarp طوال المونتاج. و`ResetTempValues` (يستدعيها `AN_ResetParkourAction` عند
`t = 1.20`) هي التي تمسحها.

**لماذا `DisableConstraint? = true`؟** حالة التسلّق تُفعّل قيد المستوى
(`Plane Constraint`) لتبقيك على وجه الجدار، والصعود يجب أن يغادر ذلك المستوى.

وحُذفت عقدة `DisengageFromState(UseAnimation = true)`. أمّا الجلوس فما زال
`UseAnimation = false` كما كان.

> صفّ `[4]` في `DPTS_ClimbTransition_SelectionTable` (DisengageFromState + Forward →
> `AM_L2H_ClimbUp`) بقي كما هو ولم يعد يُستعمل. تُرك عمداً: إن شُغّل يوماً مسار
> الإفلات المتحرّك فسيعمل الصعود عبر الجدول أيضاً بلا إعادة بناء.

### ب · الكرات الغائبة في الهواء

`E_SystemTraceOptions` — قُرئت من الأصل لا من الذاكرة:

| العدّاد | المعنى |
|---|---|
| `NewEnumerator0` | FrontalBlockSearch |
| `NewEnumerator6` | ClimbTransitionSearch |
| `NewEnumerator7` | ClimbValidations |
| **`NewEnumerator9`** | **LedgeCatchTargetSearch** *(الفهرس المعروض 8 — لا يوجد 8)* |

كان المشروع يرسم `0 , 6 , 7` فقط ⇒ **شبكة الالتقاط في الهواء لم تكن تُرسم إطلاقاً**.
وما كنت تراه في «القفز الحرّ» هو `FrontalBlockSearch` الأرضي الذي يعمل باستمرار.

أُضيف `NewEnumerator9`، وأُضيف كذلك هدف `land` إلى `TargetDebugListing` بلون أخضر
ليظهر مهبط الصعود ككرة.

**للتراجع:** احذف `NewEnumerator9` من
`DynamicParkourTraceSystem ▸ DebugSettings ▸ TraceSearchOptions ▸ SearchTypeSelection`.

### ج · تصحيح

بحثتُ أوّلاً عن `Execute In Air Searches` و`Walking Off Ledge` فلم أجد شيئاً وظننتها
ناقصة. **هي موصولة أصلاً** — عناوين العقد بلا مسافات:

```
Event OnWalkingOffLedge → UnCrouch → Execute INAIRSearches
```

فبند «الالتقاط عند المشي خارج المنصّة» من الدرس السادس مُنفَّذ منذ قبل.

الترجمة: **0 أخطاء، 0 تحذيرات.**

---

## ٢٣ · الانتقال إلى مكان آخر في العالم

### العَرَض

الصعود يشتغل لكن الشخصيّة **تُنقل إلى موضع آخر في الخريطة** بدل أن تقف فوق الحافة.

### السبب — هدف قديم لا هدف خاطئ

في `AN_FindLandTarget` كان `TraceStartTarget = height`. و`FindTargetByWarpName`
يُرجع نجاحاً فقط إذا كان المتّجه **غير صفري**:

```
Success = (الاسم موجود في WarpNames) AND (الهدف != 0,0,0)
```

وتقسيم DPTS للمتغيّرات حاسم هنا:

| المتغيّر | الفئة | من يكتبه | من يمسحه |
|---|---|---|---|
| `HeightTarget` | **TempValues** | دوالّ البحث فقط (`Calculate Corner`, `CalculateLedgeCatchAction`, …) | **`ResetTempValues`** |
| `ActiveClimbTarget` | **SaveValues** | `InitializeStateOverrides` ثمّ `UpdateActiveTarget` كل إطار | `EndStateOverrides` فقط |

وأنت متعلّق ساكن: مونتاج الإمساك انتهى بـ`AN_ResetParkourAction` ⇒ `ResetTempValues`
⇒ **`HeightTarget = (0,0,0)`**. والصعود يستدعي `TriggerActionMontage` مباشرةً
(القسم ٢٢) فلا يجري أيّ بحث يُحدِّثه.

⇒ `FindTargetByWarpName('height')` يفشل ⇒ الفرع الآخر ⇒ `TraceForTarget` لا يعمل
أبداً ⇒ **`LandTarget` يبقى على قيمته القديمة** من آخر حركة ⇒ الـwarp يسوق الشخصيّة
إلى ذلك الموضع البعيد.

> الزوايا لم تُصَب بهذا لأن `Calculate Corner` يكتب `HeightTarget` مباشرة قبل تشغيل
> المونتاج.

### العلاج

`AM_L2H_ClimbUp ▸ AN_FindLandTarget ▸ TraceStartTarget` : `height` → **`activetarget`**

`ActiveClimbTarget` هو نقطة القبض على الحافة، ويبقى حيّاً ويتتبّع زحفك طوال التعلّق.

> الدرس السادس يفعل العكس — في مونتاجات الـLedge Catch البديلة يحوّل
> `Find Land Target` من `active target` إلى `height`، لأن هناك **لا حالة تسلّق بعد**
> فلا `activetarget`. الحالتان متعاكستان، وكلٌّ يستعمل الهدف الحيّ في سياقه.

### حارس إضافي

الـwarp يعمل سواء نجح البحث أم لا، فالفشل كان يعني نقلاً عبر الخريطة. أُضيف قبل
التشغيل:

```
Set Busy = true → Set DPTS.LandTarget = DPTS.ActiveClimbTarget → TriggerActionMontage(…)
```

فإن أخفق البحث عن السطح صار أسوأ ناتج **الوقوف عند شفة الحافة** بدل الانتقال بعيداً.
وإن نجح فإنّه يكتب فوق هذه القيمة بالأرضيّة الحقيقيّة.

### إن لم يجد السطح

وسّع نطاق البحث في `AM_L2H_ClimbUp ▸ AN_FindLandTarget`:

| الحقل | الآن | معناه |
|---|---|---|
| `SearchStartOffset ▸ Up` | 20 | يبدأ التتبّع 20 سم فوق الشفة |
| `MaxLandSeachZDistance` | 90 | يتتبّع نزولاً 90 سم |
| `LandForwardPositionSearch` | 30 → 90 | يكنس أماماً من 30 إلى 90 سم بخطوة 10 |

الترجمة: **0 أخطاء، 0 تحذيرات.**

---

## ٢٤ · القفز بين الحواف (الدرس الخامس)

### أ · شبكة ATS — أرقام مؤلَّفة لا مقدَّرة

قِست مسار الجذر لأربعة عشر كليب قفز، فظهرت شبكة نظيفة تماماً:

| الكليب | الانتقال |
|---|---|
| `short` أعلى / أسفل | **± 100 سم** رأسيّاً |
| `short` يمين / يسار | **± 100 سم** جانبيّاً |
| `long` أعلى / أسفل | ± 200 سم |
| `long` يمين / يسار | ± 200 سم |
| `short_*_back` | −250 سم أماماً (القفز للخلف) |

ومنها جاء نطاق `CompareValue` مباشرةً: الكليبات القصيرة تنتقل 100 ⇒ **50 … 150**.
ليس تخميناً — هو مدى الكليب نفسه.

### ب · لا حاجة لنقل الجذر هذه المرّة

خلافاً لكليبات الصعود، كليبات القفز تحمل انتقالها **على الجذر نفسه** والحوض ساكن
بالنسبة إليه (`pelvis delta = 0.00` في المصدر والهدف). فلم يكن لدى `Pelvis Motion`
شيء يسرقه، وخرجت إعادة الاستهداف بالجذر سليماً:

```
مصدر 100.00  →  هدف 96.25   ( = 100 × 0.9625، نسبة الهيكلين نفسها)
```

فاكتفيت بـ`enable_root_motion = true` و`force_root_lock = false`.

> **ملاحظة توضّح متى يلزم النقل:** الحوض هو ما يسرقه المُستهدِف. إن كان الانتقال على
> الجذر نجا؛ وإن كان على الحوض نسبةً إلى الجذر (كما في الصعود) سُطِّح الجذر.

### ج · نافذة الحركة

الجذر يتحرّك في الكليبات الأربعة بين **الإطار 13 والإطار 36** — أي `t = 0.217 … 0.600`
في كلٍّ منها. ومنها التوقيتات:

| المُشعِر | الزمن | الضبط |
|---|---|---|
| `AN_MotionWarp_WithOffset` | 0.217 → 0.600 | هدف `height` · `UseStateWarpOverrides = True` · `Orientation = PrimaryTargetNormal` · `bIgnoreZAxis = false` |
| `AN_ResetParkourAction` | 0.62 | Blend 0.2 · **`ActivateState = True`** |

`ActivateState = True` حاسم: بدونه يخرج اللاعب إلى المشي بدل الدخول في حالة التسلّق
الجديدة على الحافة الهدف.

### د · الجدول

`DPTS_ClimbJump_SelectionTable` كان **فارغاً**. صفّ واحد `L2H` بأربعة عناصر:

| # | الاتجاه | المدى | المونتاج |
|---|---|---|---|
| 0 | Forward | 50–150 | `AM_L2H_ClimbJump_Up` |
| 1 | Backward | 50–150 | `AM_L2H_ClimbJump_Down` |
| 2 | Left | 50–150 | `AM_L2H_ClimbJump_Left` |
| 3 | Right | 50–150 | `AM_L2H_ClimbJump_Right` |

`Target` و`Active` = (LedgeClimb, FullContact) و(LedgeClimb, Hang).

> الدرس يقصرها على `FullContact` فقط. وسّعتُها لتشمل `Hang` لأن حوافنا تنتج
> الحالتين، ولأن `ClimbStates` يشغّل كليبات braced في الحالتين أصلاً. حين تُوصَل
> كليبات `bar` أعِد القصر.

وفُعِّل `AllowedActionListing ▸ ClimbJump = True`.
وإعدادات البحث (`FromClimbState ▸ JumpTargetSeachSettings`) كانت مضبوطة أصلاً على
افتراضيّات DPTS: أماماً 150 · ارتفاعاً 350 · صندوق 5×40×5 — لم تُمَسّ.

### هـ · حلّ التعارض — الصعود صار الحلّ الأخير

كان `Jump + أمام` يعترض كلّ شيء ويشغّل الصعود، فيمنع «اقفز إلى الحافة التي فوقك».
أُعيد البناء إلى تصميم DPTS الطبيعي:

```
IA_Jump ▸ Started → Branch(NOT Busy) → Switch(S_ParkourStates)
  ├ 0 (أرضاً)   → SearchForParkourAction                    ← قفز/تسلّق/إمساك (كما كان)
  └ 1,2,3,4,6   → SearchForParkourAction (عقدة ثانية)
                     → Branch( NOT DPTS.Busy )
                          ├ صحيح (لم يجد شيئاً) → الصعود
                          └ خطأ  (وجد فعلاً)    → لا شيء، الفعل يجري
```

**لماذا `Busy` هو المؤشّر؟** لأن كلّ دوالّ `Calculate*` في DPTS تضبط `Busy = true`
عند نجاحها. فإن عاد البحث و`Busy` ما زال false فمعناه أنّه لم يجد زاوية ولا حافة
للقفز إليها ⇒ الصعود هو التصرّف الصحيح.

وترتيب البحث داخلياً يحكمه `InStateActionSearchPriority = NewEnumerator1`
(ClimbTransition أوّلاً) — أي: زاوية، ثمّ قفزة، ثمّ صعود.

وحُذفت عقدتا `Equal(Byte)` و`Get ParkourInputDirection` من مسار القفز؛ والمتغيّر
`ParkourInputDirection` باقٍ (يُكتب عند `IA_Move`) لأن الاتجاهات الثمانية ستحتاجه.

### و · الخطوة اليدويّة الوحيدة

`Execute_ClimbJumpAction` حدث واجهة، وصفحات الرسم محميّة في Python فلا تستطيع أدواتي
إنشاءه (`add_event_node` ينتج CustomEvent لا تجاوزاً). في `BP_L2H_Base`:

1. لوحة **My Blueprint ▸ Interfaces ▸ Action Events** ⇒ دبل‑كليك على
   `Execute Climb Jump Action`.
2. من العقدة: اسحب مرجع `Dynamic Parkour Trace System` ⇒ `Anim Selection Climb Jump`.
3. على العقدة: `Priority = State Match Type` · `Row Use Index = 0`.
4. `Compare Values`: `Make Array` ← `Climb Jump Distance` من المكوّن.

### ز · التنقيح

أُضيف `NewEnumerator5` (**ClimbJumpTargetSearch**) إلى
`DebugSettings ▸ SearchTypeSelection`، فصارت: `0 , 5 , 6 , 7 , 9`.

الترجمة: **0 أخطاء، 0 تحذيرات.**

---

## ٢٥ · مفتاحان كانا يُلغيان القفزة بصمت

### العَرَض

«لاعب لا يقيس، إنّما يصعد عند القفز حتى لو وجّهته للجانب.»

أي أن بحث القفزة لم يكن يُنتج شيئاً أبداً، فيسقط النظام دائماً إلى الحلّ الأخير
(الصعود) — وهو سلوك صحيح لمنطقٍ لم يجد هدفاً، لكن البحث نفسه لم يكن يعمل.

### السبب الأوّل — `InStateActionSearchPriority` طرفيّ لا تسلسليّ

في `DynamicParkourTraceSystem ▸ EventGraph`:

```
Switch on E_StateActionPrioritySelection
  NewEnumerator0 (JumpAction)       → Find Climb Jump Target
  NewEnumerator1 (TransitionAction) → FindClimbTransitionTarget   ← طرفيّ، لا يكمل
```

والتسلسل في اتجاه واحد فقط:

```
Find Climb Jump Target
  Fail = false → انتهى (وجد قفزة)
  Fail = true  → Branch( ActiveParkourState == Wallrun )
                    نعم → توقّف
                    لا  → FindClimbTransitionTarget    ← الزوايا
```

وكان المشروع على **`NewEnumerator1`** — ضبطتُه في جلسة سابقة لأُشغّل الزوايا، وهو
يُلغي بحث القفزة **كلّيّاً** لأنّه لا يكمل إلى الفرع الآخر.

⇒ صار **`NewEnumerator0` (JumpAction)**. وهذا أفضل في الحالتين: يجرّب القفزة أوّلاً،
وعند فشلها يسقط تلقائيّاً إلى بحث الزوايا. والمانع الوحيد للتسلسل هو حالة
`Wallrun` — لا تخصّ الحواف.

> **تصحيح لما قلته سابقاً:** وصفتُ الترتيب بأنّه «زاوية ← قفزة ← صعود».
> الترتيب الحقيقي في الكود هو **قفزة ← زاوية ← صعود**.

### السبب الثاني — الحواف ممنوعة من البحث الجانبي

`DynamicParkourTraceSystem ▸ JumpSearchLeftRightStateMatch` — تقرؤها
`Find Climb Jump Target` و`FindBaseJumpSeachOffsets`، وهي التي تقرّر أيّ الحالات
يُولَّد لها **إزاحات بحث يميناً ويساراً**.

| قبل | بعد |
|---|---|
| (BalanceBeam, FullContact) | (BalanceBeam, FullContact) |
| (Wallrun, FullContact) | (Wallrun, FullContact) |
| — | **(LedgeClimb, FullContact)** |
| — | **(LedgeClimb, Hang)** |

`LedgeClimb` لم تكن مدرجة إطلاقاً ⇒ لا إزاحات جانبيّة ⇒ **القفز يميناً ويساراً من
حافة كان مستحيلاً** مهما كان الإدخال. وهذا هو شقّ «حتى لو وجّهته للجانب» بالضبط.

هذان المفتاحان يفسّران العَرَض كاملاً: الأوّل يقتل القفزة في كل الاتجاهات، والثاني
يقتل الجانبيّة وحدها.

### للتراجع

- `InStateActionSearchPriority` → `NewEnumerator1`
- احذف مدخلَي `LedgeClimb` من `JumpSearchLeftRightStateMatch`

الترجمة: **0 أخطاء، 0 تحذيرات.**

---

## ٢٦ · الصعود بلا موضع قدم — الحلّ الأخير كان بلا شروط

### العَرَض

«يصعد عند القفز إن لم يجد حافة جانبية، رغم أن السماكة 0.1 (لا موضع قدم) ورغم أنّي
موجَّه للجانب — يقف فوق الحافة وفوقها جدار فيعلق.»

### السبب

القسم ٢٤-هـ جعل الصعود «الحلّ الأخير»: إن عاد `SearchForParkourAction` و`Busy` ما زال
false ⇒ `AM_L2H_ClimbUp` **مباشرةً**. لا فحص للاتجاه ولا للمكان. ومع حارس القسم ٢٣
(`LandTarget = ActiveClimbTarget`) يصير الناتج عند غياب السطح: الوقوف على الشفة
داخل الجدار الذي فوقها.

### العلاج — شرطان قبل الصعود

```
… → Branch(NOT DPTS.Busy)
       └ صحيح → Branch( ParkourInputDirection == Forward )      ← جديد
                   └ صحيح → CanClimbUpFromLedge                  ← جديد
                              → Branch(CanClimb)
                                   └ صحيح → Set Busy → Set LandTarget → TriggerActionMontage(ClimbUp)
```

كل فرع «خطأ» لا يفعل شيئاً — يبقى متعلّقاً.

`BP_L2H_Base ▸ CanClimbUpFromLedge` (دالّة جديدة، مخرج `CanClimb`):

| الفحص | الشكل | الشرط |
|---|---|---|
| ١ · مساحة الوقوف | كبسولة الشخصيّة (نصف القطر والارتفاع مقروءان من المكوّن)، تبدأ فوق `ActiveClimbTarget` بـ `HalfHeight + 15` وتكنس **45 سم أماماً** (انتقال جذر `L2H_MD_climb_up` = 44.255) | **لا إصابة** |
| ٢ · موضع القدم | خطّ عند `Lip + Fwd×45` من +50 إلى −50 | إصابة |
| ٣ · السطح أفقي | `ImpactNormal.Z` | **≥ 0.7** (≈ 45°) |

القناة `Visibility`، والرسم `ForDuration` 3 ث (أحمر = لا إصابة، أخضر = إصابة) —
يُطفأ بتحويل `DrawDebugType` إلى `None` في العقدتين.

الترجمة (تحذيرات = أخطاء): **0 أخطاء، 0 تحذيرات.** والحفظ مؤكَّد من القرص
(الحجم 1,580,231 → 1,680,166، ومجلّد AuraSandbox فارغ).

### للتراجع

صِل `Branch(NOT Busy) ▸ then` مباشرةً بـ `Set Busy` واحذف العقد الخمس الجديدة
(`Get ParkourInputDirection`, `Equal(Enum)`, فرعان, `CanClimbUpFromLedge`).

---

## ٢٧ · مسبار الحواف — المرحلة ١ (كشف ورسم فقط، بلا سلوك)

كشف واحد للأعلى والجانبين والأسفل. **لا يغيّر أيّ حركة بعد** — يرسم فقط.

### الدوالّ (كلّها في `BP_L2H_Base`)

| الدالّة | الدخل → الخرج | ماذا تفعل |
|---|---|---|
| `ProbeOnInput` | DirR, DirU | تُستدعى بعد `UpdateParkourStateData` (أي في حالة باركور فقط). تشغّل المسبار **مرّة لكلّ قطاع عصا** (مفتاح = 3·sign(R)+sign(U)). العصا محايدة ⇒ `ProbeResult = 0`. |
| `ProbeLedge` | DirR, DirU | القطاع = اتجاه العصا في مستوى الجدار (يمين الممثّل × الأعلى) ±60°. 5 زوايا (كل 30°) × 3 مسافات (60/100/140 سم) = **15 نقطة** حول `ActiveClimbTarget`. |
| `ProbeSample` | P → Found, Top, Normal | ١) خطّ نحو الجدار (−40 → +80) يجب أن يصيب واجهة شبه عموديّة (`|N.z| ≤ 0.5`). ٢) كرة r=5 تسقط 8 سم داخل الواجهة من +45 إلى −10 ⇒ سطح الشفة (`N.z ≥ 0.7`، ولا بدء داخل هندسة). ٣) كرة r=8 فوق الشفة بـ20 سم ⇒ مكان لليدين. |
| `ProbeStandAt` | Top → Stand, Land | كبسولة الشخصيّة تنزل عند `Top + Fwd×45` من `HH+25` إلى `HH−25`. بدء داخل هندسة ⇒ لا مكان. إصابة بـ`N.z ≥ 0.7` ⇒ أرض، و`Land` = نقطة الإصابة. |

### التصنيف والاختيار

- المرشّح يُقبل إن كان بُعده **على اتجاه العصا > 25 سم** (يُسقط الحافة التي تمسكها).
- الدرجة = `المسافة/150 + 2×(1 − cos الزاوية عن العصا)` — الأصغر يفوز.
- **أعلى صرف** (`DirU > 0.5`, `|DirR| < 0.1`): إن كانت الحافة التي تمسكها نفسها صالحة للوقوف ⇒ **أرض** وتتقدّم على أيّ حافة أعلى.
- حافة فائزة تمرّ `ProbeStandAt` ⇒ تُرقّى إلى **أرض**.

المتغيّرات: `ProbeResult` (0 لا شيء · 1 حافة · 2 أرض) · `ProbeTarget` · `ProbeNormal` ·
`ProbeBestScore` · `ProbeLastKey` (يُصفَّر كل إطار مشي، فيعمل المسبار من جديد عند كل إمساك).

### الرسم (ثانيتان)

| الشكل | المعنى |
|---|---|
| نقطة رماديّة صغيرة | نقطة فحص لم تجد شيئاً |
| نقطة خضراء | شفة صالحة وُجدت |
| كرة **خضراء** | الهدف المختار = حافة |
| كرة **زرقاء** | الهدف المختار = أرض (صعود) |
| كرة **حمراء** أمامك بـ60 سم | لا شيء في هذا الاتجاه |

القناة `Visibility` في كل التتبّعات. الترجمة (تحذيرات = أخطاء): **0 أخطاء، 0 تحذيرات.**

### ⚠ Aura sandbox انقلب أثناء الجلسة

السجلّ: `18:23:11 LogAura: EnableSandbox: loaded 'AuraSandbox' — active.` — حفظ
`save_assets` أعاد `true` لكنّه كُتب في
`Intermediate/Sandboxes/AuraSandbox/Sandbox/Game/_Project/Characters/BP_L2H_Base.uasset`.
كان الملفّ الوحيد هناك؛ نُسخ إلى `Content/` (مطابق بايتاً ببايت، 2,197,008). النسخة
السابقة (1,680,166) محفوظة احتياطاً. **أطفئ Aura قبل أيّ حفظ قادم** وإلّا سيعود الحفظ إلى الصندوق.

### للتراجع

احذف عقدتَي `ProbeOnInput` و`Set ProbeLastKey` من `IA_Move` في `EventGraph` — الدوالّ
بلا مستدعٍ لا تفعل شيئاً.

---

## ٢٨ · المسبار صار حلقة كاملة — ولماذا فشل الأسفل

### ما كان في الصور

معظم الرسم في لقطات الفشل **ليس من المسبار**، بل من DPTS (مقروء من
`DynamicParkourTraceSystem ▸ DebugSettings`):

| الشكل | المصدر |
|---|---|
| صناديق حمراء/خضراء متتالية | `ClimbJumpTargetSearch` — بحث القفز في DPTS |
| كرة حمراء عند اليدين | هدف `initial` |
| كرة صفراء | هدف `height` |
| كرة خضراء نعناعيّة | هدف `land` |
| خطوط صفراء | `showNormals?` |

والقفز ما زال يمرّ ببحث DPTS (المرحلة ١ كانت رسماً فقط) — فهو الذي يفشل مع فارق 1 مم
وهو الذي وضع الكرة الخضراء أبعد من الحافة.

### لماذا لم يكشف المسبار ما تحتك

كان يرمي خطّاً أفقيّاً نحو الجدار عند كل نقطة ثم يبحث عن سطح **فوق** نقطة الإصابة
بـ45 سم فقط. العوارض الرقيقة العائمة (سماكة ~20 سم) تقع بين النقاط، وأدنى نقطة كانت
−140 سم فلا تبلغ عارضة عند مستوى القدمين (~−160).

### التصميم الجديد — أعمدة ساقطة لا خطوط أفقيّة

| الدالّة | ماذا تفعل |
|---|---|
| `ProbeScan` | حلقة كاملة حول اليدين في مستوى الجدار: **13 عموداً** (−150…+150 سم كل 25) × **5 شرائح ارتفاع** (+180…+100 · +100…+25 · +25…−30 للجانبين فقط · −30…−110 · −110…−200) × **عمقين** (−9 سم للحواف البارزة، +9 للمسطّحة والعائمة) = 130 تتبّعاً، مرّة عند الإمساك ومرّة كلّما تحرّكت القبضة > 10 سم. كل حافة تُحفظ في `ProbeCands` / `ProbeCandNormals` وتُرسم **نقطة بنفسجيّة**. |
| `ProbeBand` | كرة r=5 تسقط عبر الشريحة ⇒ سطح مسطّح (`N.z ≥ 0.7`، بلا بدء داخل هندسة). ثم خطّ أفقي تحت السطح بـ6 سم يقيس **الواجهة الأماميّة الفعليّة** ⇒ الشفة = (XY الواجهة، Z السطح) — فأيّ فارق عمق، حتى 1 مم، مقيس لا مفترض. ثم فحص مكان اليدين. |
| `ProbeSelect` | لا تتبّعات — يختار من المرشّحين بحسب العصا: مخروط ±60°، على الاتجاه > 25 سم، مسافة < 220 سم. الدرجة = `مسافة/100 + 3×(1 − cos)` (القريب يفوز أقوى من قبل). ثم قاعدة «أعلى صرف ⇒ أرض أوّلاً» وترقية الحافة إلى أرض كما في ٢٧. |
| `ProbeOnInput` | مسح عند تحرّك القبضة أو إمساك جديد (`ProbeLastKey = 99` أثناء المشي)؛ اختيار عند تغيّر القطاع أو بعد مسح. |

حُذفت `ProbeLedge` و`ProbeSample`.

### الرسم الجديد (ألوان لا يستعملها DPTS، مع نصّ)

| الشكل | المعنى |
|---|---|
| نقاط **بنفسجيّة** | كل الحواف الموجودة في الحلقة |
| خطّ **بنفسجي** + `PROBE: LEDGE` | الهدف المختار حافة |
| خطّ **سماوي** + `PROBE: FLOOR` | الهدف المختار أرض |
| خطّ **برتقالي** قصير + `PROBE: NONE` | لا شيء في هذا الاتجاه |

### فخّ في الأداة

`write_graph_dsl` **لا يمسح** الدالّة قبل الكتابة: بقيت في `ProbeOnInput` ثلاثون عقدة
من النسخة الأولى (منها استدعاء `ProbeLedge` المحذوفة) — غير موصولة بالتنفيذ لكن
موصولة ببيانات المدخل. حُذفت بفحص قابليّة الوصول عبر أسلاك التنفيذ.

الترجمة (تحذيرات = أخطاء): **0 أخطاء، 0 تحذيرات.** الحفظ وصل إلى `Content/`
(2,396,991) — Aura لم يعد يعترضه.

---

## ٢٩ · المرحلة ٢ — القفز صار يعمل بالمسبار

### إصلاح الجانب أوّلاً

العَرَض: «البنفسجي لا يعمل عندما تكون الحافة على الجنب». السبب: كل عمود كان يفحص
**خطّين رقيقين فقط** في العمق (−9 و+9)، والكرة r=5 كانت تصيب **زاوية** العارضة فتقرأ
normal مائلاً (< 0.7) وتُرفض.

`ProbeBand` صار **خطّاً** ساقطاً (لا زوايا)، يتبعه:
- كرة r=3 من +12 إلى +5 فوق السطح ⇒ يجب ألّا تصيب شيئاً — الخطّ الذي يبدأ داخل جسم
  صلب يعبره بصمت، وهذا الفحص يثبت أنّ فوق السطح هواء.
- قياس الواجهة الأماميّة (−60 → +40) ثم مكان اليدين، كما في ٢٨.
- ويُرجع الآن **`HitActor`**.

`ProbeScan`: **خمسة أعماق** (−12, 0, +12, +24, +36) بدل اثنين ⇒ 13 × 5 × 5 = 325 خطّاً
عند كل مسح — يُحفظ الممثّل في `ProbeCandActors`.

`ProbeSelect`: نافذة المسافة صارت **50 … 150 سم** = صفوف `DPTS_ClimbJump_SelectionTable`
(`leap_short` ينتقل 100). فلا يُختار هدف لا يبلغه أيّ مونتاج — وهذا سبب «الكرة أبعد
من الحافة».

### `HangJump` — ما يجري عند القفز وأنت متعلّق

`IA_Jump ▸ Switch(S_ParkourStates) ▸ 1,2,3,4,6` → **`HangJump`** (بدل
`SearchForParkourAction` + سلسلة `CanClimbUpFromLedge`).

```
ProbeScan → ProbeSelect(آخر عصا: ProbeDirR/U)
├ LEDGE → ParkourInputOverride(المحور الغالب للإزاحة الحقيقيّة)
│         → SetSystemUseTransform → CalculateClimbJump(Target, Normal, Actor, Transform, Initial = ProbeTarget)
├ FLOOR → Busy = true → LandTarget = ProbeTarget → TriggerActionMontage(ClimbUpMontages)
└ ثم إن بقي Busy = false:
     SearchForParkourAction   (زوايا DPTS، ومعها بحثه القديم كاحتياط)
     وإن بقي false:  أسفل ⇒ DisengageFromState(false)  (سقوط)
                     أعلى صرف ⇒ DisengageFromState(false) + LaunchCharacter(Z = 450)
                     جانب ⇒ لا شيء
```

**لماذا `CalculateClimbJump`؟** هي الدالّة التي ينتهي إليها بحث DPTS نفسه: تتحقّق من
وسوم الممثّل (`ValidateParkourStateTag`)، تحسب الـsubtype، تضبط `height` و
`HeightTargetNormal` و`ClimbJumpDistanceF` و`Busy`، ثم `Execute_ClimbJumpAction` ⇒
الجدول ⇒ المونتاج ⇒ `AN_ResetParkourAction(ActivateState = True)` ⇒ حالة التسلّق على
الحافة الجديدة. استبدلنا **البحث** وحده؛ كل ما بعده بقي DPTS كما هو.

**لماذا `ParkourInputOverride` قبلها؟** صفوف الجدول مقيّدة بـ Forward/Backward/Left/Right،
والعصا القُطريّة تُقرأ ForwardRight فلا يطابقها صفّ. نعطي DPTS المحور الغالب من
الإزاحة الفعليّة للهدف (|x| > |z| ⇒ يمين/يسار، وإلّا أعلى/أسفل). يعود الإدخال الحقيقي
في الإطار التالي من `IA_Move`.

متغيّرات جديدة: `ProbeCandActors` · `ProbeTargetActor` · `ProbeDirR/U` ·
`ClimbUpMontages = [AM_L2H_ClimbUp]` · `ProbeLastKey` افتراضيّاً 99.
حُذفت `CanClimbUpFromLedge` وعُقَدها الأربع عشرة في `IA_Jump`.

### تنظيف

`write_graph_dsl` ترك 44 عقدة قديمة في `ProbeOnInput` و91 في `ProbeSelect` — حُذفت بفحص
الوصول عبر التنفيذ.

الترجمة (تحذيرات = أخطاء): **0 أخطاء، 0 تحذيرات.** الحفظ في `Content/` (2,510,007).

### للتراجع

صِل مخارج `Switch ▸ 1,2,3,4,6` بعقدة `SearchForParkourAction` جديدة واحذف `HangJump`.

## ٣٠ · المسك المائل · الصعود رغم العائق · مدى أوسع

### أ · لماذا يبقى المسك مائلاً — دالّة DPTS بلا مستدعٍ

ثلاث قراءات متسلسلة:

1. `DynamicParkourTraceSystem ▸ SetStateRotationOverrides` تُطفئ عند بداية الحالة:
   `bUseControllerRotationPitch/Yaw/Roll = false` و`bOrientRotationToMovement = false`
   و`bUseControllerDesiredRotation = false`. وفي `BP_L2H_Base ▸ UpdateRotation_PreCMC`
   كلّ الجسم محصور في `ActiveParkourState == None`. ⇒ **لا شيء يدوّر الممثّل أثناء التسلّق.**
2. الدالّة التي كانت ستصحّحه موجودة: `ActiveClimbTransformAdjustment` — تُقحِم
   (`TInterpTo` سرعة 10) تحوُّل الممثّل نحو
   `AdjustOrientation(RotationFromXVector(ActiveClimbNormal), Yaw+180)`، وتتخطّى نفسها
   إن كان `Busy` أو الحالة Wallrun، وتشترط أن يحتوي
   `StateTransformCorrectionAllowListing` الحالةَ الحاليّة (وهو يحتويها فعلاً:
   LedgeClimb FullContact وHang وPointClimb FullContact وBalanceBeam Hang).
3. **لا أحد يستدعيها.** فُحصت كلّ رسوم DPTS المئة (لم يطابق إلّا عقدة مدخلها نفسها)،
   و`grep` على كلّ `Content/**/*.uasset` أعاد ملفّ DPTS وحده، و`ReceiveTick` غير مُنفَّذة
   على المكوّن، وشخصيّة العرض `DPTS_DEMOCharacter` بلا Tick أصلاً.

⇒ الزاوية التي تهبط بها تبقى إلى الأبد. ولأنّ `ProbeScan` يبني شبكته على
`GetActorRightVector`/`ForwardVector`، و`ProbeBand` يقيس واجهة الجدار على
`ForwardVector`، و`UpdateActiveTarget` يرمي خطّه الأمامي من تحوُّل الممثّل — فإنّ ميل
الممثّل **يُميل كلّ القياس**. وهذا هو «يدمّر الكشف كذلك».

### العلاج — `BP_L2H_Base ▸ SnapClimbRotation`

```
flat   = MakeVector(-ActiveClimbNormal.x, -ActiveClimbNormal.y, 0)   ← Z مصفَّر فلا Pitch ولا Roll
target = RotationFromXVector(flat)
if !Busy  و  |flat| > 0.01  و  الحالة ∈ {LedgeClimb, PointClimb, FreeClimb}
     SetActorRotation( RInterpTo(GetActorRotation, target, DeltaSeconds, 10) )
```

الحالة تُفحَص بـ`Switch on S_ParkourStates` (ثلاث حالات صريحة) لا بشجرة منطقيّة —
BalanceBeam FullContact مُستثنى لأنّ الوقوف على عارضة يواجه طولها لا جداراً، وWallrun
مُستثنى لأنّ اتجاهه محاذٍ للجدار.

التوصيل في `EventGraph`: `PreCMCTick → UpdateRotation_PreCMC → UpdateMovement_PreCMC →`
**`SnapClimbRotation`** (سلك واحد جديد، مؤكَّد بعد الحفظ).

**لماذا لم أستدعِ دالّة DPTS نفسها؟** لأنّها تصحّح الموضع أيضاً عبر
`Get State Warp Overrides(false)`، وهذه تطابق على **الحالة فقط** وتكسر الحلقة عند أوّل
مطابقة (مؤكَّد: `SetSuccessLocal ▸ then → Knot_5 → Knot_4 → Break` في الماكرو). الصفّ [0]
يحتوي LedgeClimb، فيُستعمل حتّى في Hang ⇒ إزاحة `-24.4 / -162.23864` بدل
`-21.5 / -152.9`، أي **نقل وضع التعلّق 9.34 سم**. فاكتفيتُ بالدوران.
تشغيلها الكامل (ليمسك الموضع أيضاً) خطوة مستقبليّة تحتاج توحيد صفوف `StateWarpOverrides`.

### ب · الصعود رغم عائق فوق الرأس — فحص فُقد في المرحلة ٢

القسم ٢٦ بنى `CanClimbUpFromLedge` بثلاثة فحوص، أوّلها **مساحة الوقوف**: كبسولة الشخصيّة
تبدأ فوق الشفة بـ`HalfHeight + 15` وتكنس 45 سم أماماً، ويجب **ألّا تصيب شيئاً**.
القسم ٢٩ حذف الدالّة واستبدلها بـ`ProbeStandAt` — التي أبقت فحص موضع القدم وأسقطت فحص
المساحة. فصار «أعلى صرف ⇒ أرض» يُقرّ الصعود حتى لو كان فوق الشفة جدار أو سقف.

أُعيد الفحص كدالّة مستقلّة `ProbeHeadroom (Top) → Clear`:

| المقدار | القيمة | من أين |
|---|---|---|
| نصف قطر الكبسولة | 30 | `BP_L2H_Base ▸ CollisionCylinder ▸ CapsuleRadius` |
| نصف ارتفاعها | 86 | `CapsuleHalfHeight` (أي ارتفاع كامل 172) |
| بداية الكنس | `Top + Z×(86+15)` = الشفة +101 | الكبسولة تشغل الشفة+15 … +187 |
| مسافة الكنس | 45 سم أماماً | انتقال جذر `L2H_MD_climb_up` = 44.255 |
| الشرط | **لا إصابة** | |

الرسم `ForDuration` 2 ث — **أخضر = المكان خالٍ، أحمر = مسدود**، فترى سبب الرفض.

تُستدعى في `ProbeSelect` عند **كلّا** موضعَي الترقية إلى «أرض»: قاعدة «أعلى صرف» وترقية
الحافة الفائزة. فإن كان الرأس مسدوداً بقي التصنيف حافة (فيقفز إلى الأعلى إن وُجدت) أو
لا شيء (فيبقى متعلّقاً) — وهو المطلوب: «يرفض الأمر ويبقى مكانه».

وأُغلق الباب الثاني: حُذف العنصر `DisengageFromState + Forward → AM_L2H_ClimbUp` من
`DPTS_ClimbTransition_SelectionTable ▸ L2H` (كان الصفّ 4 من ستّة). ذلك المسار يمرّ من
`DisengageFromState(true) → CalculateDisengageDrop` وهو حساب **سقوط**، بلا أيّ فحص
لمساحة الوقوف. الصعود الآن من المسبار وحده. بقي `DisengageFromState + None →
AM_L2H_LedgeRelease` للسقوط، والزوايا الأربع كما هي (خمسة عناصر).

### ج · «لا يقيس الحافة التي بجانبه» — شرط لم يُوصَل أصلاً

شريحة المنتصف في `ProbeScan` (+25 … −30، أي مستوى اليدين) كانت محكومة بـ:

```
(j != 2) OR ( Abs( 0.0 ) > 40 )          ← الصفر ليس خطأ قراءة: الدبّوس غير موصول
```

فالطرف الثاني false دائماً ⇒ **الشريحة لا تُفحَص إطلاقاً**، وحافة على مستوى يديك تماماً
لا تُرى أبداً. هذا هو العَرَض بعينه.

السبب أداة لا منطق: `write_graph_dsl` **يفشل صامتاً** في وصل تعبير محسوب إلى
`Math|Float|Absolute(Float)`، وكذلك إلى مقارنة استنتج نوعها `int`. كُرِّر الفشل مرّتين
بنفس الكتابة، وظهر في القراءة `(> 0 40.0)`. الحلّ: المقارنة على **دليل الحلقة** لا على
القيمة المحسوبة:

```
j != 3  OR  i < 7  OR  i > 9        ⇔   |الإزاحة الجانبيّة| ≥ 50 سم
```

(i=8 ⇒ 0 · i=7/9 ⇒ ±25 تُتخطّى · i=6/10 ⇒ ±50 تُفحَص)

### د · المدى

| | قبل | بعد | الاشتقاق |
|---|---|---|---|
| الأعمدة | 13 × 25 سم = ±150 | **17 × 25 = ±200** | يساوي سقف نافذة الاختيار |
| الشرائح | 5 (+180 … −200) | **7 (+260 … −290)** | هامش 60 فوق و90 تحت المدى، لالتقاط الشفة نفسها |
| الأعماق | 5 (−12 … +36) | كما هي | |
| خطوط لكل مسح | 325 | **590** | 17×7×5 = 595 ناقص شريحة المنتصف للأعمدة ≤25 |
| نافذة الاختيار | 50 … 150 | **50 … 200** | |
| `CompareValues` في جدول القفز | 50 … 150 | **50 … 200** | يجب أن تطابق النافذة، وإلّا `Busy = true` بلا مونتاج ⇒ تعليق |

⚠ **تحذير صريح:** `L2H_MD_leap_short_*` ينتقل 96.25 سم فعليّاً (100 × 0.9625). عند هدف
بـ200 سم نحن **نمطّط** المقطع ضعفين عبر Motion Warp — يعمل لكنّه يبدو طائراً. الحلّ
الصحيح: استرجاع مقاطع `leap_long` من ATS (المقيسة 200 سم) ومونتاجاتها وصفوف
`150 … 250`. هذه هي الخطوة التالية المقترحة.

### الترجمة والحفظ

الترجمة (تحذيرات = أخطاء): **0 أخطاء**. حُذفت 97 عقدة يتيمة في `ProbeSelect` بفحص
الوصول عبر التنفيذ (الكتابة لا تمسح القديم). الحفظ في `Content/` لا في الصندوق:
`BP_L2H_Base` 2,510,007 → **2,690,664** · جدول الانتقال 10,774 → **9,455** · جدول القفز
9,229 (نفس الحجم، والمحتوى مؤكَّد بالقراءة).

### للتراجع

| التغيير | التراجع |
|---|---|
| ميل المسك | احذف عقدة `SnapClimbRotation` من `EventGraph` (الدالّة بلا مستدعٍ = بلا أثر) |
| فحص الرأس | احذف عقدتَي `ProbeHeadroom` من `ProbeSelect` وصِل `Stand` مباشرةً بالفرع |
| الشريحة الوسطى | أعد الشرط إلى `j != 3` وحده |
| المدى | `17 → 13` و`- i 8.0 → - i 6.0` و`7 → 5` شرائح، والنافذة 200 → 150، والجدول 200 → 150 |
| صفّ الصعود | أعد عنصراً بـ`DisengageFromState / Forward / AM_L2H_ClimbUp` |

## ٣١ · أولويّة القفز من الحافة — حافة أوّلاً، ولا صعود تحت عائق

### العَرَض

«الخطّ يكشف أنّ فوقي عائقاً، ومع ذلك أصعد على الحافة تحت العائق وأعلق.»

### السبب — ترقية الحافة إلى «أرض»

`ProbeSelect` كان يرقّي **أيّ حافة فائزة** سطحُها صالح للوقوف إلى `FLOOR`. فوق `LEDGE_01`
تقع `LEDGE_JUMP_UP` (كتلة Z 350→450 متراجعة 60 سم عن الواجهة): المسبار يجدها حافةً،
يرى سطحها العلوي خالياً ⇒ `FLOOR` ⇒ `HangJump` يشغّل `AM_L2H_ClimbUp` **من شفتك** نحو سطح
الكتلة. المونتاج مصنوع لصعود 171.3 سم فوق شفتك، فيدخل تحت الكتلة ويعلق.

وفحص الرأس (القسم ٣٠-ب) لم يمنعه لأنّه كان يُفحص عند **الهدف** (فوق الكتلة = خالٍ) لا عند
شفتك (= مسدود بالكتلة نفسها). وقاعدة «أعلى صرف ⇒ أرض أوّلاً» كانت تسبق البحث عن حافة،
والمخروط ±60° كان يرفض حافة علويّة مائلة قليلاً.

### الترتيب الجديد

```
العصا أعلى (DirU > 0.1):
  ١ حافة في النصف العلوي (Δz > 25) ضمن 50…200، الأقلّ درجةً   ⇒ 1 LEDGE      → CalculateClimbJump
  ٢ ProbeStandAt(شفتك) و ProbeHeadroom(شفتك)                   ⇒ 2 CLIMB UP   → AM_L2H_ClimbUp
  ٣ ProbeJumpClear                                               ⇒ 3 FREE JUMP  → Disengage + Launch(0,0,450)
  ٤ وإلّا                                                        ⇒ 4 BLOCKED    → يبقى
العصا أسفل (DirU < −0.1):
  ١ حافة في النصف السفلي (Δz < −25) ضمن 50…200                 ⇒ 1 LEDGE
  ٢ وإلّا                                                        ⇒ 3 FREE DROP  → Disengage
العصا جانب:
  ١ حافة في المخروط ±60° (along > 25، cos > 0.5) ضمن 50…200    ⇒ 1 LEDGE
  ٢ وإلّا                                                        ⇒ 0 → زوايا DPTS (SearchForParkourAction) أو يبقى
```

الدرجة كما هي: `المسافة/100 + 3×(1 − cos)` مع اتجاه **العصا** (لا الكاميرا — يعمل لكلا
اللاعبين). الحافة المائلة تفوز فقط إن كانت أقرب بوضوح؛ أبعد من 200 لا تُختار أصلاً.

**حُذفت ترقية الحافة إلى أرض نهائيّاً**: الحافة تبقى قفزة حافة دائماً، والصعود لا يكون
إلّا على **شفتك أنت** وبعد التأكّد أنّ فوقها خالٍ.

`SearchForParkourAction` لم يعد يُستدعى إلّا عند `ProbeResult ∈ {0, 1}` وبقاء `Busy = false`
(جانب بلا حافة، أو حافة رفضتها وسوم DPTS) — فلا يسبق القفز الحرّ.

### `ProbeJumpClear () → Clear` — جديدة

كبسولة الشخصيّة (30 / 86) من `ActorLocation − Forward×30` (نصف قطر للخلف حتى لا تبدأ
داخل الجدار) إلى الأعلى **103 سم**:

```
ارتفاع القفزة الحرّة = v² / 2g = 450² / (2 × 980) = 103.3 سم
v = 450  : LaunchVelocity.Z في HangJump
g = 980  : الجاذبيّة الافتراضيّة × GravityScale 1.0 (EventStateResetExecution)
```

إصابة ⇒ `BLOCKED` ويبقى متعلّقاً بدل أن يرتطم.

### الرسم

| النصّ | اللون | المعنى |
|---|---|---|
| `PROBE: LEDGE` | بنفسجي | قفزة إلى حافة |
| `PROBE: CLIMB UP` | سماوي | صعود ووقوف على شفتك |
| `PROBE: FREE JUMP` / `FREE DROP` | برتقالي | قفز/سقوط حرّ |
| `PROBE: BLOCKED` | أحمر | عائق — يبقى |
| `PROBE: NONE` | برتقالي قصير | جانب بلا حافة |

وكبسولتا `ProbeHeadroom` و`ProbeJumpClear`: أخضر = خالٍ، أحمر = مسدود.

### الترجمة والحفظ

الترجمة (تحذيرات = أخطاء): **0 أخطاء**. حُذفت 110 عقدة يتيمة في `ProbeSelect`. كلّ شروط
الحلقة مقروءة بعد الكتابة وموصولة (لا قيم حرفيّة). الحفظ في `Content/`:
`BP_L2H_Base` 2,690,664 → **2,892,931**.

### للتراجع

`ProbeSelect` و`HangJump` يُعادان إلى نصّ القسم ٣٠ (السجلّ يحمل الشكل السابق)؛
`ProbeJumpClear` بلا مستدعٍ إن أُعيدا = بلا أثر.

## ٣٢ · القفز الطويل — مقاطع 200 سم بدل تمطيط مقطع 100

### لماذا

منذ القسم ٣٠ صارت نافذة الاختيار 50 … 200، لكنّ المونتاجات الأربعة مبنيّة على
`L2H_MD_leap_short_*` التي تنتقل **96.25 سم** فقط. فأيّ حافة بين 100 و200 كانت تُمطّط المقطع
حتى ضعفيه عبر Motion Warp — يعمل لكنّه يبدو طائراً.

### القياس قبل العمل (المصدر في `Content/AdvancedTraversalSystem/.../Leap/Braced/`)

| المقطع | الإطارات | الطول | انتقال الجذر |
|---|---|---|---|
| `MD_leap_long_r_up` | 90 | 1.5 ث | (0, 0, **+200**) |
| `MD_leap_long_r_down` | 90 | 1.5 | (0, 0, **−200**) |
| `MD_leap_long_l_left` | 90 | 1.5 | (**+200**, 0, 0) |
| `MD_leap_long_r_right` | 90 | 1.5 | (**−200**, 0, 0) |
| `MD_leap_short_r_up` (مرجع) | 90 | 1.5 | (0, 0, +100) |

### الاسترجاع

`batch_retarget_animations` بالمسترجِع نفسه `RTG_ALS_to_UEFN`، **بلا `overwrite`** (الأهداف غير
موجودة أصلاً) ⇒ 4 مقاطع في `_Project/Parkour/Anims/ATS/`. القياس بعده:

| المقطع | الانتقال | التحقّق |
|---|---|---|
| `L2H_MD_leap_long_*` | **192.493** | = 200 × 0.962466 (نسبة الارتفاع ALS→UEFN نفسها) |
| إطارات الحركة | **13 → 36** | مطابقة للقصير تماماً |

الجذر نجا في الاسترجاع (كالقصير) — لا حاجة لزرع. الأعلام كالقصير:
`enable_root_motion = true` · `force_root_lock = false` · `root_motion_root_lock = AnimFirstFrame`.

### المونتاجات

`AM_L2H_ClimbJump_{Up,Down,Left,Right}_Long` = **نسخة** من المونتاج القصير المقابل مع استبدال
مرجع المقطع فقط. النسخ آمن هنا لأنّ الطولين متطابقان (1.5) — فخّ «الطول المخزَّن» لا يقع.
والإشعارات كما هي لأنّ نافذة الحركة متطابقة:

| الإشعار | الزمن | = |
|---|---|---|
| `AN_MotionWarp_WithOffset` (height، `UseStateWarpOverrides`) | 0.217 مدّة 0.383 | الإطارات 13 → 36 عند 60 إطاراً/ث |
| `AN_ResetParkourAction` (`ActivateState = True`) | 0.62 | |

### الجدول `DPTS_ClimbJump_SelectionTable ▸ L2H` — ثمانية عناصر

| المدى | المونتاج | الاشتقاق |
|---|---|---|
| **50 … 145** | `AM_L2H_ClimbJump_*` (قصير) | |
| **145 … 200** | `AM_L2H_ClimbJump_*_Long` | 145 ≈ منتصف الانتقالين (96.25 + 192.49) / 2 = 144.37 |

فأقصى تمطيط الآن: القصير عند 145 = ×1.51 · الطويل عند 145 = ×0.75 (ضغط) · الطويل عند 200 = ×1.04.

`ClimbJumpDistanceF` الذي يقارنه الجدول = `Distance(TargetLocal, ActiveClimbTarget)` (مقروء من
`CalculateClimbJump`) — **المقياس نفسه** الذي يختار به `ProbeSelect`، فلا فجوة بين النافذتين.

الحفظ في `Content/` (4 مقاطع ~700 KB، 4 مونتاجات ~14 KB، الجدول 9,229 → 14,119).
`ASSET_LICENSES.md`: صفّ المشتقّات صار 28 كليباً.

### للتراجع

احذف العناصر 4…7 من صفّ `L2H` وأعد `Max` في الأربعة الأولى إلى 200.

## ٣٣ · القفز القُطري يحتاج خمس ضغطات — المسبار كان يختار طرف الحافة

### العَرَض

«كشف الحواف القُطريّة (بين الجنب والأعلى) ضعيف، وأحياناً يحتاج خمس ضغطات ولا يقفز.»
في اللقطة: `PROBE: LEDGE` وخطّ بنفسجي، والكرة الحمراء (`initial`) عند **طرف** العارضة الأقرب.

### السبب — فحص العرض في `CalculateClimbJump`

بعد التحقّق من الوسوم، تستدعي `CalculateClimbJump` الدالّة `DPTS_BFL ▸ GetTargetMidPoint`
بعرض `SpaceValidationWidth = 40` (مقروء من `DPTS_Settings_SelectionTable ▸ NewRow ▸ LedgeClimb`):
تتتبّع على امتداد الحافة يميناً ويساراً من الهدف، وتشترط أن يكون ما وجدته **أعرض من 40 سم**
(`Distance2D(A, B) > 40`)، ثم تنقل الهدف إلى المنتصف. **إن فشلت لا تفعل شيئاً** — `Busy` يبقى
false ولا مونتاج.

والهدف القُطري هو بطبيعته **أقرب نقطة** من عارضة جانبيّة-علويّة = **طرفها**. فعند الطرف لا
يوجد عرض على جانب واحد ⇒ الفحص يفشل ⇒ يسقط `HangJump` إلى بحث DPTS القديم ⇒ نتيجة عشوائيّة.
وكلّ ضغطة يتغيّر الإمساك قليلاً فيتغيّر المرشّح — لذلك «أحياناً بعد خمس ضغطات».
الأهداف المستقيمة لا تتأثّر لأنّها تقع في منتصف الحافة.

### العلاج — `ProbeWidthOK (Lip, Normal) → OK`

```
edge = Normalize( Normal × Z )                 ← اتجاه امتداد الحافة
in   = Lip − Normal × 5                         ← 5 سم داخل سطح الشفة
خطّ عمودي من +10 إلى −10 عند  in + edge × 25   ← يجب أن يصيب
خطّ عمودي من +10 إلى −10 عند  in − edge × 25   ← يجب أن يصيب
```

وجود الحافة على **±25 سم** يضمن `A–B ≥ 50 > 40` ⇒ فحص DPTS ينجح دائماً.
يُستدعى في `ProbeSelect` فقط للمرشّح الذي **يحسّن الدرجة** (لا لكلّ المرشّحين). وبما أنّ المرشّحين
كلّ 25 سم على امتداد الحافة، فإن رُفض الطرف فاز المرشّح التالي نحو الداخل.

⇒ حافة أقصر من 50 سم لن تُختار أبداً — وهذا صحيح، فـDPTS كان سيرفضها على أيّ حال.

الترجمة (تحذيرات = أخطاء): **0 أخطاء**. حُذفت 146 عقدة يتيمة في `ProbeSelect`.

## ٣٤ · تصحيح ٣٣ — الرفض ألغى التنقّل، فصار «إزاحة نحو المنتصف»

### ما حدث

«ألغيتَ التنقّل بين الحواف وصار كلّه قفزاً حرّاً.» فحص القسم ٣٣ (`ProbeWidthOK`) كان يشترط
وجود الحافة على **±25 سم** من الهدف. وقراءة المرحلة (`DefaultLevel`) تُظهر أنّ معظم عوارض
المسار `Bar_Seg_*` طولها **50 سم بالضبط** (مثلاً `Bar_Seg_10`: X 140→190) وعمقها 10 —
فالنقطتان ±25 تقعان على الحافّتين أو خارجهما ⇒ كلّ عارضة رُفضت ⇒ لا حافة ⇒ قفز حرّ.
خطئي: وضعتُ حدّاً دون أن أقيس أطوال القطع في المرحلة نفسها.

### العلاج — لا رفض، بل إزاحة

- حُذفت `ProbeWidthOK`، وعاد فلتر الاختيار كما في القسم ٣١ تماماً.
- بعد اختيار الحافة: يُجمع كلّ مرشّح على **نفس خطّ الشفة** قرب الهدف
  (`|Δz| < 5` و`|Δ·Normal| < 5` و`Distance2D < 60`) ويُنقل الهدف إلى **متوسّطها** (XY)
  مع إبقاء Z. متغيّران جديدان: `ProbeSpanSum` (Vector) و`ProbeSpanN` (int).
  - عارضة 50 سم ⇒ الهدف قرب منتصفها ⇒ فحص DPTS (عرض > 40 حوله) ينجح.
  - طرف عارضة طويلة ⇒ الهدف ينتقل ~25 سم للداخل.
  - الأهداف المستقيمة في منتصف حافة ⇒ المتوسّط ≈ الهدف نفسه.

### حماية من التعليق

الإزاحة (±30 سم) قد تُخرج المسافة من صفوف الجدول، و`CalculateClimbJump` يضع `Busy = true`
**قبل** اختيار المونتاج ⇒ بلا صفّ مطابق يعلق الشخص. لذلك وُسِّع طرفا الجدول:
القصير **0 … 145**، الطويل **145 … 250** (الطويل عند 250 = تمطيط ×1.30 فقط).
نافذة الاختيار في المسبار بقيت 50 … 200 قبل الإزاحة.

الترجمة (تحذيرات = أخطاء): **0 أخطاء**. الحفظ في `Content/`.

## ٣٥ · القفز القُطري — فلتر نصف الدائرة، ومقاطع قُطريّة حقيقيّة

### السبب الباقي في الفلتر

`ProbeSelect` (القسم ٣٤) كان يقسم العصا بعتبة `|DirU| > 0.1`: أيّ مكوّن رأسي فوقها ⇒ فرع
«أعلى» الذي يشترط `Δz > 25`. فالعصا القُطريّة (DirU = 0.707)، أو عصا يمين مائلة قليلاً
(DirU = 0.15)، ترفض حافة جانبيّة على الارتفاع نفسه ⇒ لا حافة ⇒ **صعود أو قفز حرّ** بدل
القفز الجانبي. هذا هو «القُطري ضعيف» و«الأولويّة» معاً.

### الفلتر الجديد (بزاوية العصا لا بمكوّنها)

```
s = sin(زاوية العصا)          ← _returnvalue_3، مقروء وموصول
مقبول ⇔ 50 ≤ المسافة ≤ 200  و  along > 25  و  ( cos > 0.5  أو  |s| ≥ 0.5 )
```

- العصا **جانبيّة غالباً** (|s| < 0.5، أي ضمن 30° من الأفق) ⇒ مخروط ±60° كما كان.
- غير ذلك (أعلى، أسفل، **قُطري**) ⇒ **نصف دائرة** حول اتجاه العصا (`along > 25`).
  للأعلى الصرف `along = Δz` ⇒ مطابق للشرط القديم `Δz > 25`.
- الدرجة كما هي: `المسافة/100 + 3×(1 − cos)`.

### البدائل حين لا حافة

| العصا | قبل | بعد |
|---|---|---|
| s ≥ 0.5 (أعلى، أعلى-قُطري) | صعود ← قفز حرّ ← مسدود | كما هي |
| s ≤ −0.5 (أسفل، أسفل-قُطري) | سقوط حرّ | كما هي |
| جانب مائل قليلاً (0.1 < s < 0.5) | **صعود/قفز حرّ** ✗ | 0 ⇒ زوايا DPTS أو يبقى ✓ |

### مقاطع قُطريّة

`E_DirectionSelection` فيه ثمانية اتجاهات (`ForwardRight`, `ForwardLeft`, `Backwardright` — بهذا
الحرف الصغير في الأصل —, `BackwardLeft`)، وATS فيه قفزات قُطريّة بزاوية 45° تماماً:

| المقطع (المصدر ← المسترجَع) | الانتقال | الإطارات |
|---|---|---|
| `leap_short_{r_up_right, l_up_left, r_down_right, l_down_left}` | 100 ← **96.23** (68.05 × √2) | 13 → 36 |
| `leap_long_…` نفسها | 200 ← **192.46** (136.09 × √2) | 13 → 36 |

الجذر نجا في الاسترجاع، والأعلام كبقيّة المقاطع. المونتاجات الثمانية
`AM_L2H_ClimbJump_{UpRight,UpLeft,DownRight,DownLeft}[_Long]` = نسخ من `Up`/`Down` مع استبدال
المقطع؛ الإشعارات صحيحة لأنّ نافذة الحركة واحدة (warp 0.217 مدّة 0.383، reset 0.62 — مقروءة).

الجدول `L2H`: **16 عنصراً** — 8 اتجاهات × (قصير 0…145، طويل 145…250).

### `HangJump` — الاتجاه من الهدف

```
off = ProbeTarget − ActiveClimbTarget ;  lat = off · Right ;  vz = off.z
X = lat² > 0.1716 × vz²  ?  sign(lat) : 0
Y = vz²  > 0.1716 × lat² ?  sign(vz)  : 0          0.1716 = tan²(22.5°)
ParkourInputOverride(X, Y)
```

ثمانية قطاعات متساوية 45°: هدف بين 22.5° و67.5° ⇒ مقطع قُطري؛ وإلّا المستقيم. والعتبة نفسها
التي يستعملها `GetDirectionFromInput` في DPTS (يطلب تطابقاً تامّاً، لذلك الصفوف القُطريّة إلزاميّة
قبل إرسال اتجاه قُطري — وإلّا `Busy` بلا مونتاج).

الترجمة: **0 أخطاء**. ملفات DPTS الأصليّة لم تُمسّ (`git status` نظيف لها).

## ٣٦ · القفز من الأرض «يطير» بين منصّتين — حدّ المسافة في جدول التخطّي

### العَرَض

«من هذه المنصّة إلى الأخرى وكأنّه طار بينهما» — ليس زيادة قليلة بل مبالغ فيها، وفي عدّة أماكن.

### السبب

القفز من الأرض يمرّ بـ`SearchForParkourAction` ⇒ `EventExecute_VaultMantleAction` ⇒
`AnimSelection_VaultMantle(صفّ 0)` مع `[HeightF, LengthF, Dist2D]`. صفّ `DEMOUSE` يقارن الثلاثة،
لكنّ **الثالث (`Dist2D`) = 0…0 في كلّ العناصر**، و`Validate_CompareValues` (مقروء) يَعُدّ `Max == 0`
نجاحاً ⇒ **المسافة لا تُفحص أبداً** ⇒ منصّة على بُعد أمتار تطلق `Mantle`، ونافذة الالتواء الأولى
تمطّ المسافة كلّها في زمنها القصير.

قياس المقاطع (الجذر، 30 إطاراً/ث، إشعارات مقروءة):

| المونتاج | الالتواء الأوّل | الجذر عند نهايته = الاقتراب الطبيعي |
|---|---|---|
| `Mantle_1_0_run` | 0.083 → 0.224 ث | **~116 سم** (مقطع 73 سم في 0.14 ث) |
| `Hurdle_1_0_run` (L/R) | 0.083 → 0.495 | **~300 سم** |
| `Climb_Start_2_5_run` | 0.064 → 0.667 | **~210 سم** |

منصّة على 4 م مع `Mantle` ⇒ ~357 سم في 0.14 ث ≈ **25 م/ث** — هذا «الطيران».

### العلاج — صفّ خاصّ لا تعديل للأصل

- صفّ جديد `L2H` في `DPTS_VaultMantle_SelectionTable` (فهرس **1**) = نسخة `DEMOUSE` مع
  `Dist2D`: `Mantle ≤ 200` · `Hurdle ≤ 350` · `Climb_Start ≤ 320`. `DEMOUSE` لم يُمسّ.
- `BP_L2H_Base ▸ EventGraph ▸ AnimSelection_VaultMantle ▸ RowUseIndex` 0 → **1** (مقروء بعد الترجمة).
- أبعد من الحدّ ⇒ لا عنصر ⇒ `ResetSearch` (مقروء في `AnimSelection_VaultMantle`) ⇒ **قفزة GASP
  عاديّة** — لا تعليق.

`PrintString` في `EventExecute_VaultMantleAction` يطبع `Dist2D` على الشاشة 5 ث؛ إن بقي
تجاوز، القيمة المطبوعة هي ما يُضبط عليه الحدّ.

## ٣٧ · القفز من العارضة — مقاطع التعلّق الحرّ (`Hang`)

كان القفز من حالة `(LedgeClimb, Hang)` (العوارض) يشغّل مقاطع `Braced` — القدمان على جدار
غير موجود. ATS فيه `Leap/Freehang/MD_bar_leap_*`.

### الأسماء تكذب — القياس

| في ATS | الانتقال الحقيقي |
|---|---|
| `short_l_left`, `short_r_up_right`, `short_l_up_left`, `short_l_down_left` | **100** |
| `short_r_up`, `short_r_down`, `short_r_right`, `short_r_down_right` | **150** (نُسخ من long) |
| كلّ `long_*` (8 اتجاهات) | **150** |

⇒ 12 مقطعاً فريداً مُسترجَعاً: الأربعة ذات 100 (⇒ **96.25**) والثمانية `long` (⇒ **144.37**).
الجذر يحمل الانتقال، والحوض بلا انجراف (0.0)، والإطارات 13 → 36 كالبقيّة ⇒ المونتاجات
`AM_L2H_BarJump_*` نسخ من `ClimbJump` بإشعاراتها.

### الجدول `DPTS_ClimbJump_SelectionTable ▸ L2H` — 28 عنصراً

| العناصر | `Active` | المدى |
|---|---|---|
| 16 للجدار (`ClimbJump_*`) | **FullContact** فقط (كانت FullContact + Hang) | كما هي |
| يسار · أعلى-يمين · أعلى-يسار · أسفل-يسار | **Hang** | 0…120 قصير · 120…250 طويل |
| أعلى · أسفل · يمين · أسفل-يمين | **Hang** | 0…250 طويل (لا مقطع 100 حقيقي) |

120 ≈ (96.25 + 144.37) / 2. التقسيم حسب `Active` يغطّي ما كان يغطّيه `{FullContact, Hang}`
حرفيّاً، فلا حالة خرجت من التغطية.

## ٣٨ · تصحيح ٣٦ — السبب ليس المسافة بل «فجوة تُقرأ عائقاً»

### وصف المستخدم الدقيق

١. واقف على عائق، وبجانبه عائق أعلى على بُعد ~2 م. يقفز من أقرب نقطة ⇒ **دون توجيه** ينتقل
فوق العائق الثاني ويهبط بعيداً.
٢. عائقان على المستوى نفسه بينهما فجوة. عند حافّة الأوّل يقفز ⇒ ينتقل إلى **منتصف** الثاني.
٣. عائق طويل وأمامه قصير ⇒ قفز عادي. (الضابط: لا فجوة تحت المسار.)

### السبب

من حافّة منصّة، تتبّع DPTS الأمامي يصيب **جانب** المنصّة التالية، ويقيس `HeightF` من قاع
الجدار الذي أصابه (أرض الفجوة) لا من قدمَي اللاعب ⇒ الفجوة = «جدار» بارتفاع المنصّة ⇒
`Mantle`، ثمّ `AN_FindLandTarget` + الالتواء الثاني يضعانه على سطح الهدف — منتصفه.
الفجوة 2 م < حدّ القسم ٣٦ (200)، لذلك لم يمنعها.

### العلاج — `VaultGapAhead () → Gap` (في `BP_L2H_Base`)

```
feet = ActorZ − CapsuleHalfHeight
P    = Actor + Forward × Dist2D − Forward × 20      ← 20 سم قبل الجدار الذي أصابه DPTS
خطّ عمودي عند P من feet+20 إلى feet−60 (مرسوم 3 ث: أحمر بلا إصابة = فجوة)
Gap  = لا إصابة
```

أرض أدنى من القدمين بأكثر من 60 سم أمام «العائق» ⇒ ليس عائقاً على أرضي بل فجوة.

### التوصيل في `EventExecute_VaultMantleAction`

```
PrintString → VaultGapAhead → Branch(Gap)
   ├ True  → DPTS.ResetSearch → Jump                 ← قفزة GASP عاديّة
   └ False → AnimSelection_VaultMantle(صفّ 1)
                → Branch(DPTS.Busy) ─ False → Jump   ← لا عنصر مطابق (حدّ Dist2D) ⇒ قفز عادي
```

`ResetSearch` (مقروء): `IsTransitioning = false` + `ResetTempValues` (فيها `Busy`).
`Jump` مرّتين في الإطار نفسه غير ضارّ (`JumpMaxCount = 1`).
حدود `Dist2D` من القسم ٣٦ باقية حمايةً ثانية.

الترجمة (تحذيرات = أخطاء): **0**. أوّل ترجمة حذّرت «VaultGapAhead pruned» لأنّ الدالّة ذات
دبابيس تنفيذ وكانت خارج السلسلة — وُضعت في السلسلة.

## ٣٩ · تصحيح ٣٨ — الفحص مبنيّ على هدف `height` لا على `Dist2D`

### ما قيل وما قيس

تحليل خارجي نسب «الطيران» إلى `LengthF` وهدف `length`. قِيس ذلك في المشروع:

| الادّعاء | القياس | الحكم |
|---|---|---|
| المونتاجات تلتوي إلى `length` | `RootMotionModifier_SkewWarp.WarpTargetName`: الأوّل **`height`**، الثاني **`land`** (Mantle وHurdle وClimb_Start) | ✗ |
| `land` = الحافّة البعيدة | `AN_FindLandTarget`: يبدأ من **`height`** (+20 أعلى) ويمشي للأمام **30…90** (Mantle) · **120…250** (Hurdle) · **40…120** (Climb_Start) | ✗ |
| غيّر شرائح `LengthF` | الهبوط لا يعتمد على `LengthF` | لا يُصلح |
| `JumpZVelocity 500` هو السبب | قفزة بلا عصا لا تتحرّك أفقيّاً؛ «دون توجيه» ⇒ مونتاج | ليس السبب |
| `VaultGapAhead_0` فارغة | صحيح — بقيّة `remove/add` | حُذفت |

القيم المطبوعة بعد إصلاح ٣٨ (22:25–22:32): `HeightF ≈ 99.85` دائماً (كتل «1 M»)، `Dist2D` 30…170،
`LengthF` حتى ~350. ⇒ الطيران 2 م لا يمكن إلّا إذا كان **`height` نفسه على الكتلة التالية**:
الالتواء الأوّل (0.14 ث) يسحبه عبر الفجوة إلى حافّتها، ثمّ `land` 30…90 سم على سطحها
= «منتصف العائق الآخر». وفحص ٣٨ كان يعتمد على نقطة واحدة من `Dist2D` — غير موثوق.

### `VaultGapAhead` الجديدة

```
T     = DPTS.GetHeightTarget                  ← الحافّة التي سيلتوي إليها فعلاً
v     = (T − Actor) مسطّحاً ،  end = v − Normalize(v)×30     ← 30 سم قبل الحافّة
4 خطوط عموديّة عند Actor + end × {0.25, 0.5, 0.75, 1.0}
     من feet+20 إلى feet−60 (مرسومة 3 ث)
rise  = T.z − feet
Gap   = (أيّ خطّ بلا إصابة)  أو  (rise < 40)
```

- الحالة ١ (عائق أعلى بعد فجوة): خطّ فوق الفجوة بلا إصابة ⇒ قفز عادي.
- الحالة ٢ (المستوى نفسه): `rise ≈ 0 < 40` ⇒ قفز عادي (وأيضاً خطّ الفجوة).
- عائق على أرضي: الخطوط الأربعة تصيب أرضي و`rise ≥ 40` ⇒ Mantle كما هو.

30 سم قبل الحافّة لأنّ خطّاً يبدأ داخل جسم الكتلة لا يُصيب (يُقرأ فجوة كاذبة).
الإصابات موصولة من `ReturnValue` (لا `OutHit`) — مقروء. الترجمة **0**.

## ٤٠ · السبب الحقيقي للطيران — «القفز الديناميكي» في DPTS (مُثبت في PIE)

### القسمان ٣٦–٣٩ عالجا مساراً لم يكن يعمل

أُعيد إنتاج الحالة ٢ في `DefaultLevel`: `LevelBlock_Traversable24` (X 2400→3000) و`25`
(X 3400→4000)، كلاهما 250 سم، فجوة 400. اللاعب على حافّة الأوّل (X 2960، اتجاه +X)، `IA_Jump`
بلا عصا:

| القياس | القيمة |
|---|---|
| الهبوط | X **3772–3822** (مرّتين، حتميّ) — 862 سم، و422 داخل الكتلة الثانية |
| سطر `HeightF=` في السجلّ | **لا شيء** ⇒ `EventExecute_VaultMantleAction` لم يُستدعَ أصلاً |
| DPTS `Busy` وكلّ الأهداف | false / صفر طوال القفزة |
| `CharMoveComp.MovementMode` | `MOVE_Falling` — قفزة فيزيائيّة، لا مونتاج |
| `Velocity.X` | **603.19 ثابتة** من أوّل إطار (بلا عصا، أكبر من MaxWalkSpeed 500) |
| نفس القفزة في منتصف الكتلة (X 2700) | عموديّة، X يبقى 2700 ⇒ السلوك مرتبط بالحافّة |

البحث عن `SuggestProjectileVelocity` في الأصول ⇒ `DynamicParkourTraceSystem ▸ DynamicJump`
(مستدعاة من `CalculateDropAction` فقط):

```
DynamicJump(JumpTarget, JumpBoost):
  v = SuggestProjectileVelocity_CustomArc(Actor, JumpTarget)
  LaunchCharacter(v × (1 + JumpBoost), XY override, Z override)
```

`CalculateDropAction`: عند الحافّة، إن وُجد `JumpTarget` (سطح أمامي) و`UseDynamicJump?` ⇒
`DynamicJump`، وإلّا `SearchFailExecution → JumpOverride → Jump()` (قفزة عاديّة).

الإعدادات (`DPTS_Settings_SelectionTable ▸ NewRow ▸ fromGroundState ▸ SecondarySearchSettings ▸
JumpFromDropOffSettings`):

| الحقل | القيمة | الأثر |
|---|---|---|
| `DynamicJumpSettings.UseDynamicJump?` | **true** | يُطلق نحو أيّ سطح يجده |
| `MaxSearchForwardDistance` | **450** | فجوات حتى 4.5 م |
| `MaxSearchHeight` | **250** | وسطح أعلى حتى 2.5 م — الحالة ١ |
| `DynamicJumpSettings.JumpBoostMult` | **0.3** | السرعة ×1.3 ⇒ يتجاوز الحافّة إلى منتصف الكتلة — الحالة ٢ |

### العلاج

- صفّ جديد `L2H` (فهرس 1) في `DPTS_Settings_SelectionTable` = نسخة `NewRow` بفرق **واحد**
  (مقارنة آليّة لكلّ الحقول): `UseDynamicJump? = false`. `NewRow` لم يُمسّ.
- `BP_L2H_Base ▸ DynamicParkourTraceSystem ▸ SettingsUseIndex` 0 → **1** (مقروء بعد الحفظ).

### التحقّق (PIE، نفس الموضع)

`MOVE_Falling` بـ`Velocity = (0, 0, ~500)`، القمّة Z 454 (≈116 سم فوق السطح)، الهبوط **X 2960**.

### عن القسمين ٣٨–٣٩

`VaultGapAhead` لم تكن سبب هذه الحالة (مسار Mantle لم يُستدعَ). تبقى حارساً لمسار Mantle
عبر فجوة (قيم `HeightF` السابقة تُثبت أنّه مسار موجود)، ولا تمسّ Mantle على الأرض نفسها.

### إن أردتَ القفز الديناميكي لاحقاً

أعِد `UseDynamicJump? = true` في صفّ `L2H` مع `JumpBoostMult = 0` ومسافة أقصر — أو اجعله
مشروطاً بدفع العصا نحو الفجوة. الآن: لا قفز موجَّه تلقائي أبداً.

## ٤١ · إعادة تفعيل القفز الديناميكي للأهداف العالية فقط — وقياس ما يحدث أمام الجدران

### الإعداد (صفّ `L2H` في `DPTS_Settings_SelectionTable`، `JumpFromDropOffSettings`)

| الحقل | ٤٠ | ٤١ |
|---|---|---|
| `UseDynamicJump?` | false | **true** |
| `JumpBoostMult` | 0.3 | **0** (لا تجاوز للهدف) |
| `MaxSearchForwardDistance` | 450 | **200** |
| `SearchStartingZOffset` | −40 | **+130** |

`Drop/JumpTargetSearch` (مقروء): حلقة من الفهرس 0 إلى `MaxSearchHeight/10`، خطّ أفقي كلّ 10 سم
من `DropStartTargetLocal` (حافّة المنصّة) + `SearchStartingZOffset`، وتتوقّف عند **أوّل** إصابة.
بدء البحث عند +130 يتجاهل كلّ ما دون ارتفاع القفزة العادية (منصّة بنفس المستوى لا تُصاب).

### قياسات PIE (مشاهد مؤقّتة في عالم اللعب فقط، لا تُكتب على القرص)

| المشهد | النتيجة |
|---|---|
| حافّة، منصّة بنفس المستوى بعد فجوة 400 | قفزة عاديّة، X لا يتغيّر (10285) ✓ |
| حافّة، جدار 500 بعد فجوة 120 | قفزة عاديّة — يمرّ بمسار Mantle و`VaultGapAhead` تحوّله لقفز عادي |
| أرض، جدار 200 على بُعد 200 | Mantle/Climb_Start يتسلّقه (Busy، HeightF 199.7) |
| أرض، جدار 300 على بُعد 200 | يتسلّقه ويقف عليه (Z 388) |
| أرض، جدار 450 (بلا وسم) على بُعد 200 | قفزة عاديّة، لا Mantle (HeightF 0) |
| أرض، `LEDGE_01` (350 + وسم climb) على بُعد 150 و300 | قفزة عاديّة، لا Mantle |

### الخلاصة

- **القفز الديناميكي لا يعمل إلّا عند حافّة فيها هبوط**، ولا يُستدعى إن وُجد جسم أمامي ضمن 250 سم
  (`FrontalBlockSearch` يأخذ مسار Mantle). لذلك بحدّ أمامي 200 يكاد لا يُطلَق أبداً.
- **الجدران ≤ 300 تُتسلَّق أصلاً** (HighMantle / Climb_Start_2_5). الفجوة الفعليّة: جدار ≥ ~350 من الأرض.
- قفزة «اصعد الجدار العالي وامسك حافّته» تحتاج ميزة جديدة في `BP_L2H_Base` (إطلاق نحو الجدار بسرعة
  رأسيّة محسوبة `v = √(2g·h)` ثمّ التقاط الحافّة في الهواء)، لا مجرّد ضبط جدول.

## ٤٢ · قفزة الجري المبالغ فيها — قياس ثمّ ضبط `JumpZVelocity` و`GravityScale`

### القياس قبل التعديل (PIE، أرض مستوية، جري بالعصا ثمّ `IA_Jump`)

| | القيمة |
|---|---|
| سرعة الجري | 500 (ثابتة قبل القفز وأثناءه) |
| `JumpZVelocity` / الجاذبيّة | 500 / 1.0 |
| سرعة الصعود عند الانطلاق | 480 |
| القمّة | ~127 سم |
| زمن الطيران | ~1.02 ث |
| المسافة الأفقيّة | **~510 سم** = 500 × 1.02 |
| السرعة الأفقيّة أثناء القفز | 500 ثابتة ⇒ لا تدخّل من DPTS |

المسافة = السرعة × (2 × JumpZ ÷ (980 × Gravity)). الطيران ليس خطأ في DPTS بل فيزياء الأرقام.
لم أحصل على قياس نظيف لقفزة جري عند حافّة بالتوقيت (محاولة اصطدمت بحاجز 1 م حقيقي عند X 3100 على
`LEDGE_01`، وأخرى فاتها توقيت الحافّة لاختلاف زمن اللعب عن الزمن الحقيقي)، لكنّ الجدول لا يبحث عن
أهداف بمستوى الحافّة بعد ٤١، فلا سبب لاختلافها.

### الاختيار (المستخدم): «متوسّط»

`JumpZVelocity` **500 → 420** · `GravityScale` **1.0 → 1.2**. مكانان:
- `BP_L2H_Base ▸ CharMoveComp` (مقروءان في PIE بعد الحفظ: 420 و1.2).
- `EventGraph ▸ Set GravityScale` (يعيد ضبطها عند كلّ `EventStateResetExecution` — كانت 1.0 وكانت
  ستُبطل التعديل بعد أوّل فعل باركور).

### القياس بعد التعديل (نفس الاختبار)

| | متوقّع | مقيس |
|---|---|---|
| زمن الطيران | 2×420 ÷ (980×1.2) = 0.714 ث | ≈ 0.7 ث (12.60 → 13.33) |
| القمّة | 420² ÷ (2×1176) = 75 سم | 75 سم (Z 163 فوق أرض 88) |
| مسافة الجري | 500 × 0.714 = 357 سم | ≈ 357 سم |

### أثر جانبي

`HangJump` (قفز حرّ من التعلّق) يطلق بـ Z=450: القمّة صارت 450² ÷ (2×1176) = **86 سم** (كانت 103).
`ProbeJumpClear` يكنس 103 سم — أشدّ قليلاً من اللازم (آمن، يرفض أحياناً ما كان سيمرّ).

## ٤٣ · القفزة المطوّلة + التقاط الحافة في الهواء

### أ · القفزة المطوّلة

`JumpZVelocity` صار يُحسب كلّ إطار في `BP_L2H_Base ▸ UpdateMovement_PreCMC` (بعد `Set MaxWalkSpeedCrouched`):

```
JumpZVelocity = ( VSizeXY(GetVelocity) > 350 ) ? 470 : 420
```

العقد: `GetVelocity → VSizeXY → float>float (B=350) → SelectFloat (A=470, B=420) → Set JumpZVelocity (self = CharacterMovement)`.
كلّ الوصلات والقيم الافتراضيّة مقروءة بعد الترجمة. الجري = 500 سم/ث ⇒ يتجاوز 350؛ الوقوف 0.

| | قياس PIE | متوقّع |
|---|---|---|
| واقف: القمّة | ~73–75 سم | 75 (420) |
| جاري: سرعة الصعود عند الانطلاق | 441 بعد ~25 ملّي ثانية ⇒ 470 | 470 |
| جاري: القمّة | ~94 سم (Z 182 فوق أرض 88) | 470² ÷ (2×1176) = 94 |
| جاري: زمن الطيران | ~0.77–0.8 ث | 0.80 |
| جاري: المسافة الأفقيّة | ~400 سم | 500 × 0.80 |

(الجاري كان 75 سم / 357 سم في ٤٢.)

### ب · التقاط الحافة إذا قصرت القفزة

آليّة الالتقاط في الهواء **موجودة أصلاً** (`ExecuteINAIRSearches` ◂ `AnimSelection_LedgeCatch`) وتعمل
على الحواف الموسومة `climb`. الحدّ: `LedgeCatchSettings` (`DPTS_Settings_SelectionTable ▸ L2H`).

قياس PIE على `LEDGE_02` (وجهها X=4000، القمّة Z=350)، سقوط عموديّ بلا عصا:

| المسافة الأفقيّة عن الوجه | قبل (أمام 90 / رأسي 90) | بعد (أمام **160** / رأسي **130**) |
|---|---|---|
| 60 سم | يلتقط ويتعلّق (Z 276، ActiveClimbTarget = القمّة) | — |
| 135 سم | **لا يلتقط**، يسقط للأرض | **يلتقط ويتعلّق** (X 3964) |

الأنيميشن بعد الالتقاط هو نفسه المعرَّف للحافة (تعلّق/تسلّق حسب الوسم) — لم يُمسّ.

**تنبيه:** الالتقاط يشترط الوسم `climb`. كتل النموذج الأوّليّة بلا وسم (مثل «2.5 M» في اللقطة) لا تُلتقط
مهما اتّسع المدى؛ ضع الوسم `climb` + `ledge` على الحافّة المطلوبة.
مخاطرة: مدى أوسع يعني تعلّقاً أكثر عند القفز فوق حافّة موسومة بدل الهبوط على سطحها.

## ٤٤ · التقاط GASP في الهواء للحواف بلا وسم — فحص، وتجربة، وتعطيل

### ما وُجد في GASP (`AC_TraversalLogic ▸ TryTraversalAction`)

- **مداه في الهواء 75 سم** (كبسولة r30/hh86، نهاية +50 Z)، وعلى الأرض 75→350 حسب السرعة الأماميّة
  (`BP_L2H_Base ▸ GetTraversalCheckInputs`).
- يتعرّف على العائق بـ**فئته لا بوسم**: `Cast To LevelBlock_Traversable` ثمّ `Get Ledge Transforms`
  (كتل النموذج «2.5 M»/«1 M» من هذه الفئة). أيّ ممثّل آخر ⇒ `CastFailed` ⇒ يعود بلا فعل.
- **معطّل في مشروعنا**: عقدة `Branch(CanPerformJumpAction) → TryTraversalAction` في `EventGraph` بلا مدخل
  تنفيذ، و`CanPerformJumpAction` تعيد `false`؛ زرّ القفز يذهب إلى DPTS.
- من الأرض: الجري + القفز نحو `LevelBlock_Traversable25` (2.5 م) يصعد فوقه (Z 338 = 250 + 88)
  عبر DPTS، لا GASP.

### القياس في الهواء (قبل أيّ تعديل)

سقوط على 60 سم من وجه كتلة 250 عند مستوى اليد: كتلة `LevelBlock` عاديّة **وكتلة `LevelBlock_Traversable`**
كلتاهما لا تُلتقطان — تسقط إلى الأرض. (الالتقاط الموسوم `climb` يعمل: ٤٣.)

### التجربة

وُصل `TryTraversalAction` في `UpdateMovement_PreCMC` بعد ضبط `JumpZVelocity`، بشرط
`IsFalling ∧ ¬DoingTraversalAction ∧ ¬DPTS.Busy`، ومدى الهواء 75 → 140 سم.

**النتيجة (`LevelBlock_Traversable25`، وجهها X=3400، القمّة Z=250):** الفعل **يبدأ** (لقطة PIE: وضعيّة
`Climb_Start_2_5` ويدان على الجدار، سرعة ~236 نحو الوجه) لكنّه **لا يكتمل**: بعد ~0.15 ث تنعدم السرعة
ويسقط اللاعب إلى الأرض عند X≈3369 (من ارتفاعات مركز 150 و200 و240 — نفس النتيجة). السبب لم يُحدَّد
(مرشّحان: شرط `E_TraversalBlendOutCondition` في إشعار المونتاج يقطعه لأنّ وضع الحركة ما زال `Falling`،
أو تعارض مع إعادة ضبط وضع الحركة).

### الحالة الآن

الوصلة **مفصولة** (`Branch.execute` بلا مدخل) — لا أثر على اللعب. العقد باقية في
`UpdateMovement_PreCMC`. `GetTraversalCheckInputs` مدى الهواء 140 (غير مستعمل ما دامت الوصلة مفصولة).

## ٤٥ · الخيار أ: التقاط GASP في الهواء — تشخيص وحالته النهائيّة

### ما ثبت (PIE، `LevelBlock_Traversable25` وجهها X=3400 وقمّتها Z=250)

- GASP فيه مونتاجات **Catch** مخصّصة للالتقاط من الهواء (`Traversal/Catch/{Cliff,Hurdle,Mantle,Vault}`).
  المختار يتبع ارتفاع العائق النسبيّ: Z مركز 150 (ارتفاع 186) ⇒ `Catch_Cliff_high_standF`؛
  Z مركز 300 (ارتفاع 36) ⇒ `Catch_Mantle_low_stand` (StartTime 0.9).
- `TryTraversalAction` من الهواء **يبدأ فعلاً**: وضع الحركة `Flying`، سرعة نحو الوجه ~223، ولقطة PIE تُظهر
  الشخصيّة تعتلي الحافّة.
- المونتاج **لا يُقطع**: طباعة على `OnBlendOut`/`OnInterrupted`/بعد `PlayMontage`: `Start` ثمّ `BlendOut`
  بعد **2.62 ث** (= 3.67 − 0.9 − التمازج) — طوله الطبيعي؛ `Interrupted` لم يُطبع.
- **لكنّ وضع الحركة يعود من `Flying` إلى `Falling` بعد ~0.3–0.5 ث والمونتاج ما زال يعمل**؛ فتسقط
  الكبسولة (الكتلة مُتجاهَلة أثناء الفعل) إلى الأرض داخل الكتلة، ثمّ تُدفع خارجها عند نهاية الفعل.
  المُغيِّر لم يُعزَل. ليس DPTS (`Busy = false` طوال الوقت)، وليس إشعار BlendOut (يبدأ عند
  1.7–2.5 ث من المونتاج ولا يعمل إلّا مع مُدخل حركة).

### إصلاح حقيقي وُجد (باقٍ)

`AC_TraversalLogic ▸ PerformTraversalAction_Clients`: عقدة `Branch` كانت يتيمة (مدخلها بلا وصلة) فيُشغَّل
`PerformTraversalAction` مرّتين على الشخصيّة نفسها. وُصلت: `Cast → Branch(IsLocallyControlled) ─else→ Set TraversalResult`.
لم يغيّر النتيجة (السقوط باقٍ) لكنّه سليم شبكيّاً. **تعديل على أصل GASP.**

### ب: بحث DPTS الأرضي من الهواء

تجربة: سقوط 25 سم قبل الوجه عند مستوى اليد + `IA_Jump` ⇒ لا شيء (`Busy` false، `HeightF` 0، سقط).
مسار DPTS الأرضي لا يعمل خارج الحالة الأرضيّة.

### الحالة الآن

- وصلة الخطّاف في `BP_L2H_Base ▸ UpdateMovement_PreCMC` **مفصولة** (العقد باقية، بلا أثر).
- طباعة التشخيص أُزيلت من `AC_TraversalLogic` والوصلات الأصليّة أُعيدت (مقروءة بعد الترجمة).
- `GetTraversalCheckInputs` مدى الهواء 140 (غير مستعمل ما دام الخطّاف مفصولاً).

## ٤٦ · الخيار أ: سبب قلب وضع الحركة إلى Falling (مُقاس)

### التتبّع (سجلّ اللعب، طباعة مؤقتة أُزيلت)

```
t+0.000  MODECHG 1→3           (الشخصيّة تسقط بعد النقل)
t+0.015  SETMODE_CMC 5          (AC_TraversalLogic يطلب Flying)   ثمّ MODECHG 3→5
t+0.285  DPTSDBG caller = EndStateOverrides
         DPTSDBG BaseMovementOverrides called                       ثمّ MODECHG 5→3   ← القلب
t+1.0    MODECHG 3→1            (هبوط على الأرض داخل الكتلة)
t+2.6    SETMODE_CMC 1          (نهاية المونتاج الطبيعيّة)
```

- **`DynamicParkourTraceSystem ▸ EndStateOverrides`** يُستدعى بعد 0.285 ث من بدء فعل GASP، فيستدعي
  `BaseMovementOverrides` الذي يضبط `SetMovementMode` (+ تفعيل التصادم) — فيتحوّل `Flying ← Falling`
  ويُعاد تفعيل التصادم بينما المونتاج يعمل. لا استدعاء من `SetMovementMode_CMC` في تلك اللحظة.
- الصلة: `EndStateOverrides` تصله `DisengageFromState` و`Replication ▸ RPCMC_ResetState`
  و`WallrunGraph` و`CalculateBeamAutoEnd`. **لم يُحدَّد أيّ منها** (لا عقدة في DPTS تستدعي
  `RPCServer_ResetState` بالاسم؛ ولا شيء في `BP_L2H_Base` سوى `DisengageFromState` عند مدخل
  الانحناء/القفز من التعلّق). DPTS يربط أيضاً `OnMontageBlendingOut` عامّاً (`MontageEnd` ⇒
  `ResetWarpTargets`/`ResetSearch`) — أرجّح أنّه يتفاعل مع انتهاء تمازج مونتاج آخر (0.25 ث تمازج
  ≈ 0.285 ث)، لكنّه لا يستدعي `EndStateOverrides` بنفسه، فالربط ناقص.

### ما تبقّى

الحلّ الجذري يتطلّب إمّا (١) تحديد من يستدعي `EndStateOverrides` وتعطيله أثناء `DoingTraversalAction`
(قد يعني تعديل DPTS)، أو (٢) بناء التقاط خاص بنا يتحكّم بوضع الحركة بلا مرور بـDPTS.

### الحالة

كلّ الطباعة أُزيلت والوصلات الأصليّة أُعيدت (مقروءة). الخطّاف مفصول. ملفّ DPTS الأصلي يظهر معدَّلاً
في git بضع مئات من البايتات (ضجيج قراءة/حفظ) — يُستعاد بعد إغلاق المحرّر.

## ٤٧ · الالتقاط من الهواء لعوائق GASP غير الموسومة (يعمل)

### الحلّ (داخل `BP_L2H_Base ▸ UpdateMovement_PreCMC` فقط، بلا مساس بـDPTS)

بعد `Set JumpZVelocity`:

```
Branch( DoingTraversalAction AND IsFalling )
   then  → CharacterMovement.SetMovementMode(Flying) → Branch الخطّاف
   else  →                                            Branch الخطّاف
Branch الخطّاف( !DoingTraversalAction AND !DPTS.Busy AND IsFalling )
   then  → AC_TraversalLogic.TryTraversalAction( GetTraversalCheckInputs )
```

- الحارس الأوّل يُعيد `Flying` كلّ إطار طالما GASP في فعل عبور — فلا يهمّ مَن يستدعي
  `DPTS ▸ EndStateOverrides` (المستدعي ما زال مجهولاً؛ راجع §٤٦). الخطّاف الثاني (وُصل من جديد) هو
  ما يبدأ العبور عند السقوط، ومسافة الأمام في `GetTraversalCheckInputs` (فرع Falling) = 140.
- التراجع: فصل `K2Node_VariableSet_7.then` عن `K2Node_IfThenElse_1.execute`.

### القياس (PIE، كتلة `LevelBlock_Traversable` غير موسومة 400×400×200، إطلاق 500/420)

| الحالة | النتيجة |
|---|---|
| على مدى القفزة | التقاط → Flying كامل المونتاج → يقف فوق الكتلة (z≈288) |
| قصير بـ63 سم | التقاط وصعود |
| قصير بـ143 سم | التقاط وصعود |
| قصير بـ177 سم | لا التقاط، سقوط عاديّ (لا التقاط كاذب) |

الحدّ العملي ≈ 140–175 سم أمام الحافة. لم يُختبر بعد: جري+قفز حقيقيّان بالإدخال، اللاعب الثاني،
وكتل بارتفاعات أخرى.

## ٤٧ · الالتقاط الهوائي يعمل + بدء قفزة الجدار العالي (3.5 م)

### الالتقاط الهوائي لـ GASP (خيار ٢: حارس وضع الحركة)
- السبب (§٤٦): DPTS `EndStateOverrides ▸ BaseMovementOverrides` يعيد الوضع إلى `Falling` بعد ~0.285 ث من بدء فعل GASP.
- الحلّ (في `BP_L2H_Base` فقط، بلا مساس بـDPTS): في `UpdateMovement_PreCMC` بعد `Set JumpZVelocity`:
  `Branch(DoingTraversalAction AND IsFalling) → SetMovementMode(Flying)` ثمّ الفرع القديم `TryTraversalAction`.
  الخطّاف موصول الآن. للتراجع: افصل `K2Node_VariableSet_7.then` عن `K2Node_IfThenElse_1`.
- `GetTraversalCheckInputs` (فرع Falling): `TraceForwardDistance` 140 ← **70** بطلب المستخدم.
- القياس (كتلة Traversable ارتفاعها 200، جريٌ 500، إطلاق بسرعة قفزة): قصر 63 سم ⇒ التقاط وصعود (z 288)؛
  قصر 143 سم ⇒ التقاط (قبل التخفيض)؛ قصر 177 سم ⇒ لا التقاط. بلا هذا الحارس كان السقوط عبر الكتلة.

### الجدار العالي 3.5 م — القياس الأوّل
- جدار `LevelBlock` بلا وسم: 300 سم ⇒ `HighMantle` (Climb_Start_2_5) يصعد (z 388)؛ 330/340/350 ⇒ لا شيء (قفزة عادية ثمّ سقوط).
- جدار `LevelBlock_Traversable` 350 ⇒ لا شيء أيضاً (مُختار GASP لا يحوي فئة تسلق عالٍ).
- رفع سقف `HighMantle` في `DPTS_VaultMantle_SelectionTable` (Height 350→450 والثالثة 320→420) **لم يغيّر** النتيجة ⇒ القيد ليس في الجدول. أُعيد الجدول لقيمه الأصلية.
- حادثة: `set_data_table_rows` بمفاتيح خاطئة أفرغ الصف `L2H` وحفظه؛ استُعيد حرفياً من النصّ المقروء قبلها (انظر الذاكرة).
- ما تبقّى: تحديد سبب رفض DPTS لما فوق ~3.2 م، أو بناء ركلة الجدار (ATS WallRunning ▸ Start) كما في الخطّة.

### ٤٧-ب · البحث عن سقف ~3.2 م في DPTS (الخيار الأوّل) — لم يُحسم
- جُرِّبت (وأُعيدت لأصلها): سقف `HighMantle` (الارتفاع 350→450، القيمة الثالثة 320→420) + `FrontalBlockingObjectSearchSettings.MaxSearchHeight` 350→550، بتشغيل PIE جديد بعد كلّ حفظ ⇒ جدار 350 ما زال بلا استجابة.
- ⇒ القيد ليس في أيّ رقم من الجدولين؛ هو في منطق `FrontalBlockSearch` (حلقة كنس تصاعديّة `LoopTrace`، عدّاد `Truncate(MaxHeight ÷ (Scale.Z×2))`، فحص `InitialGap`، وفحوص الفراغ `CrossTrace`).
- قراءة متغيّرات DPTS (`HeightF`…) بعد القفز أعطت أصفاراً (تأخّر الأداة ~0.3 ث وإعادة الضبط) ⇒ لم يمكن تحديد مرحلة الفشل من الخارج. لقطات المشهد لا تُظهر خطوط تتبّع DPTS.
- التوصية: الخيار الثاني (ركلة الجدار الخاصّة) بدل تعديل منطق DPTS.

## ٤٨ · مرحلة `Reach` أثناء التعلّق (الحافة / Braced) — نظر ومدّ اليد نحو اتجاه القفز

**الدافع:** `ClimbStates` كانت ثلاث حالات فقط (`HangIdle`·`ShimmyRight`·`ShimmyLeft`)؛ فيبقى اللاعب بوضعيّة التعلّق الثابتة
حتى تنطلق القفزة كمونتاج كامل، بلا مرحلة «يتهيّأ نحو الهدف». وATS تحوي المرحلة جاهزةً (`Reach`) ولم نكن نستعملها.

### الأصول (14، كلّها على القرص ومحفوظة، `_Project/Parkour/Anims/ATS/`)
- 12 كليباً مستهدَفاً بـ`RTG_ALS_to_UEFN` (بلا `overwrite`): `L2H_MD_reach_idle_{l,r}` و`L2H_MD_reach_long_{l_left,l_up,l_down,l_up_left,l_down_left,r_right,r_up,r_down,r_up_right,r_down_right}`.
  الكليبات في المكان (لا حركة جذر) فلم يلزم ضبط الجذر.
- `L2H_Reach_L_BS` (Angle −180…0) و`L2H_Reach_R_BS` (Angle 0…180)، المحور Y = `Distance` 0…100؛ العيّنات مطابقة لمصدر ATS:
  Distance=0 ⇒ وضعيّة الخمول؛ Distance=100 ⇒ مدّ كامل بالاتجاه (أعلى 0 · يسار −90 · أسفل ±180).

### `ABP_L2H_Base`
- متغيّرات: `ReachAngle` · `ReachDistance` · `ReachTarget` (فئة `L2H | Climb`).
- `EventGraph` بعد `Set ClimbInputRight`: `Cast To BP_L2H_Base` ثمّ
  `active = (ProbeResult==1 OR ProbeResult==2) AND (|ProbeDirR|+|ProbeDirU| > 0.1) AND IsClimbing`؛
  `ReachTarget = active ? 100 : 0` · `ReachDistance = FInterpTo(ReachDistance, ReachTarget, dt, 8)` ·
  `ReachAngle = active ? DegAtan2(ProbeDirR, ProbeDirU) : ReachAngle` (تثبت الزاوية عند الإفلات فلا يتأرجح الذراع).
  فشل الـCast يكمل المسار كما كان.
- `ClimbStates`: حالتان جديدتان `ReachLeft` / `ReachRight` (مشغّل blendspace، X←`ReachAngle`، Y←`ReachDistance`)
  و10 انتقالات: `HangIdle`·`ShimmyRight`·`ShimmyLeft` ⇒ كلتا الحالتين بشرط `ReachTarget>50` ثمّ علامة `ReachAngle` (≤0 يسار، >0 يمين)،
  والخروج إلى `HangIdle` بشرط `ReachTarget<50 AND ReachDistance<3`، وبين الحالتين بعلامة الزاوية.
  الترجمة نظيفة بلا تحذيرات.

### القياس (PIE، `LEDGE_02`، حالة التسلّق `ActiveParkourState=1`)
عصا للأعلى: `ClimbStates = ReachLeft` · `ReachAngle = 0` · `ReachDistance = 100` · `ReachTarget = 100` (لقطة: الذراعان مرفوعتان نحو الهدف).
بعد ترك العصا: `HangIdle` · `ReachDistance = 0`.

### ما لم أختبره / حدود
- اتجاهات اليمين/اليسار/القُطر لم تُختبر حيّاً (لم يتوفّر هدف جانبيّ في مشهد الاختبار) — تُجرَّب باللعب.
- الاعتماد على `ProbeResult` (1 = قفزة حافة، 2 = صعود)؛ الحالات 3 (قفز حرّ) و4 (مسدود) تُبقي الوضعيّة عاديّة عمداً.
- الـshimmy (حركة DPTS الجانبيّة) ما زال يتحرّك الجسم أثناء مدّ اليد جانباً — قد يبدو انزلاقاً؛ يُحكم عليه باللعب.
- نسخة العارضة (`Bar`/Freehang) والكليبات `In`/`Out` والاتجاه الخلفي: لاحقاً.

## ٤٩ · مونتاج الإفلات عند الجلوس + فحص «اليمين لا يعمل» في Reach

### الإفلات (`BP_L2H_Base ▸ EventGraph ▸ IA_Crouch ▸ Started`)
- `DisengageFromState(UseAnimation? = true)` **لا يفعل شيئاً** حتى في تعلّق حقيقي: `IsDrop?` و`Busy` يبقيان false
  (مسار `CalculateDisengageDrop` ◂ `Execute_ClimbTransitionAction` لا يصل؛ لم أحدّد السبب داخل DPTS). أُعيد `false`.
- الحلّ: `Switch(1,2,3,4,6)` ◂ **`PlayAnimMontage(AM_L2H_LedgeRelease)`** ◂ `Delay 1.0` ◂ `Branch(ActiveParkourState != None)` ◂ `DisengageFromState(false)`.
  الإفلات الفعلي يقع من داخل المونتاج (إشعار `AN_ResetParkourAction`: `ActivateState=False` ثمّ Falling)؛ والـDelay+Branch شبكة أمان
  تُفلت يدويّاً بعد ثانية إن لم يُشغَّل المونتاج. (ملاحظة الاختبار: زرّ الجلوس `C`؛ حقن `IA_Crouch` مباشرة لا يُطلق `Started`.)
- القياس (PIE، `LEDGE_02`، حالة 1/1): بعد `C` بـ0.34 ث ⇒ `ActiveParkourState=0`، لقطة وضعيّة التعلّق وهي تتحرّر، ثمّ هبوط على الأرض.
- الجلوس ما زال يطلق أيضاً `IA_Crouch ▸ Triggered` (يُنزل الكبسولة بعد الهبوط — سلوك سابق لم يُمسّ).

### «اليمين في Reach لا يعمل»
- **لم أستطع إعادة إنتاجه**: في PIE مع تعلّق عند منتصف الحافة، عصا يميناً ⇒ `ClimbStates=ReachRight`، `ReachAngle=90`، `ReachDistance=100`،
  ويساراً ⇒ `ReachLeft` (لقطتان: كلٌّ يمدّ ذراعه نحو جهته)؛ مقاطع R وblendspace R سليمة ومتناظرة مع L (مقاسة).
- المسبار متماثل في المنتصف (`ProbeResult=1` لكلا الجهتين، الهدف ±75 سم). الـReach مشروط بوجود **هدف قفز** في الاتجاه (`ProbeResult` 1 أو 2)،
  فإن لم يوجد هدف يمين (طرف حافة، أو بلا حافة أخرى ضمن 50…200 سم) لا يظهر الـReach — وهو الأرجح.

## ٥٠ · تراجع عن مونتاج الإفلات + تثبيت زمنيّ للـReach

- **الإفلات أُعيد كما كان** (بطلب المستخدم: بعد النزول بالجلوس يبقى اللاعب جالساً): حُذفت عقد `PlayAnimMontage`/`Delay`/`Branch` ووصل
  `Switch(1,2,3,4,6) ◂ DisengageFromState(UseAnimation? = false)` مباشرةً. مقروء بعد الحفظ.
- **الـReach يتقطّع «أحياناً» جهة اليمين** (لم يُعَد إنتاجه): أُضيف تثبيت زمنيّ في `ABP_L2H_Base ▸ EventGraph`: متغيّر `ReachHold`
  (0.35 ث) يُملأ كلّما كان الشرط `active` صحيحاً ويتناقص بـ`DeltaTime`؛ `ReachTarget = ReachHold > 0 ? 100 : 0`.
  فسقوط `ProbeResult` لإطار أو اثنين (إعادة الفحص عند تحرّك الجسم أو تغيّر قطاع العصا) لا يقطع الوضعيّة. الزاوية تثبت كما كانت.
  قياس: عصا يميناً ⇒ `ReachRight` · `ReachAngle=90` · `ReachDistance=100` · `ReachHold=0.35`.
- لو بقي الانقطاع أطول من 0.35 ث فالسبب أنّ الهدف يختفي فعلاً (طرف الحافة أو أقلّ من 50 سم) لا تذبذباً.

## ٥١ · الـshimmy يدور في مكانه عند طرف الحافة ⇒ يُربط بالحركة الفعليّة

**السبب:** انتقال `HangIdle ⇒ ShimmyRight/Left` كان على العصا فقط (`ClimbInputRight > 0.1`). عند طرف الحافة يتوقّف DPTS عن تحريك الجسم
لكنّ الأنيميشن يبقى «ينزلق» في مكانه. وكان الـReach الجانبيّ يظهر أثناء الانزلاق أيضاً لأنّ المسبار يعدّ امتداد الحافة نفسها هدفاً.

**الإصلاح (`ABP_L2H_Base`):**
- متغيّر `ClimbSpeed` = `VSizeXY(GetVelocity)` من الشخصيّة كلّ إطار (مقاس: 90 أثناء الانزلاق، 0 عند الطرف).
- `HangIdle ⇒ Shimmy*`: `ClimbInputRight ≷ ±0.1 AND ClimbSpeed > 20`. `Shimmy* ⇒ HangIdle`: `|ClimbInputRight| < 0.1 OR ClimbSpeed < 5`.
- شرط الـReach: يُمنع الجانبيّ الصرف (`|ProbeDirU| < 0.5`) ما دام الجسم يتحرّك (`ClimbSpeed > 20`)؛ العموديّ والقُطريّ كما كانا.

**القياس (PIE، `LEDGE_02` من y=−60، عصا يميناً):** أثناء الحركة `ShimmyRight` · `ClimbSpeed=90` · `ReachTarget=0`؛
عند الطرف (y=90) `ReachRight` · `ClimbSpeed=0` · الهدف `StaticMeshActor_2` (y=247). اليسار لم يُقَس (حقن القيم السالبة بأداة monolith لم يُطبَّق).
ملاحظة أدوات: أدوات `unreal_editor` (Aura) توقّفت (اشتراك)؛ الاختبار بـ`pie_call_function` + `pie_inject_input_action` + `sample_pie_anim_instance`.

### فحص مقطعَي القفز على الجدار (جذر مقاس)
| المقطع | البداية | الارتفاع | النهاية |
|---|---|---|---|
| `M_L2H_Climb_2_5_run_F_Lfoot` | جري من −388 سم أمام الجدار، z=0 | يصعد 250 بحلول 2.0 ث | واقف فوق، +49 |
| `M_Neutral_Traversal_Catch_Cliff_high_stand` | **يبدأ 12.8 م فوق** (y −445، z 1279) — سقوط | يمسك الشفة عند ~1.0 ث (اليد z≈307) | واقف فوق 300، +60 |
الأوّل يناسب «جري ثمّ دفع ثمّ صعود» (يحتاج warp عموديّاً ×1.4 لـ3.5 م)؛ الثاني التقاط أثناء **السقوط من الأعلى**، لا قفز إلى جدار.

## ٥٢ · فقدان عمل الـReach واسترجاعه (Teleport auto-stash)

**ما حدث:** في 2026-10-01 الساعة 19:19 أنشأ `git stash` باسم `On clean: Teleport auto-stash` (`.git/logs/refs/stash`) لقطةً
لكلّ التغييرات غير المودَعة ثمّ أعاد الشجرة إلى `33e74744`. المحرّر أعاد تحميل `ABP_L2H_Base` من القرص فضاعت منه حالات الـReach
ومتغيّراتها، وحُذفت مقاطع `L2H_MD_reach_*` و`L2H_MD_bar_reach_*` والـBS الأربعة و`AM_L2H_Climb_3_5_run_F_Lfoot`، وعاد السجلّ
إلى ما قبل القسم ٢٩. `BP_L2H_Base` لم يتأثّر (بقي في الذاكرة وحُفظ لاحقاً بكلّ تعديلاته). الإيداع `c05e3513` حمل الـABP القديم.

**الاسترجاع:** من `stash@{0}`: الملفّات الـ29 الناقصة، والسجلّ والدليل و`ASSET_LICENSES.md`. الـABP مقفل في المحرّر، فاستُبدل من داخله:
`EditorLoadingAndSavingUtils.unload_packages` ⇒ نسخ الملفّ ⇒ `reload_packages`. التحقّق: `ClimbStates` = 5 حالات و16 انتقالاً، ترجمة نظيفة،
وفي PIE عند طرف `LEDGE_02` مع العصا يميناً: `ReachRight` · `ReachTarget=100` · `ReachAngle=90`.

## ٥٣ · بعد اللقطة: القفز للخلف، إصلاح الجلوس بعد الإفلات، القفز إلى جدار 3.5 م

- **القفز للخلف** (`HangJump`، فرع الأسفل/الحرّ): `LaunchCharacter(Forward·−400 + (0,0,350))` ثمّ تدوير الجسم 180°. مقاس: سرعة X −467، yaw 180، 230 سم للخلف.
- **الإفلات بالجلوس:** `IA_Crouch Started` ⇒ `SkipCrouchToggle=true` ⇒ `AM_L2H_LedgeRelease` ⇒ `Delay 1.0` ⇒ `DisengageFromState(false)`؛
  و`Triggered` يتخطّى قلب الجلوس مرّة واحدة. مقاس: يهبط واقفاً (`bIsCrouched=false`)، والجلوس على الأرض يعمل.
- **قفزة الجدار 3.5 م** — دالّة `WallRunTry` في `BP_L2H_Base`، تُستدعى من `IA_Jump` (حالة الأرض) قبل `SearchForParkourAction`؛
  إن صار `DPTS.Busy` صحيحاً لا يُبحث بـDPTS.
  - الشروط: على الأرض · سرعة أفقيّة > 250 · خطّ أمامي 250 سم من (z−40) يصيب واجهة قائمة (`|N.z| < 0.3`) ·
    خطّ نازل من 25 سم خلف الواجهة بين (القدم+430) و(القدم+150) يصيب سطحاً مستوياً (`N.z > 0.7`) بلا تداخل ابتدائيّ ·
    الارتفاع فوق القدم بين 300 و400.
  - التنفيذ: `HeightTarget = (IP1.x, IP1.y, IP2.z)` · `HeightTargetNormal = N1` · `Busy=true` ·
    `TriggerActionMontage([AM_L2H_Climb_2_5_run_F_Lfoot], DisableConstraint=true)`.
  - خرج `Started` يعود دائماً `false` (عقدة الإرجاع الثانية لم تقبل الخانة) ⇒ المستدعي يقرأ `Busy`.
  - **القياس (PIE، `LevelBlock_Traversable13` بلا وسوم، الواجهة y=−700، القمّة 350):** جري −Y بسرعة 500، الضغط عند y=−506
    ⇒ `Busy` ⇒ يصعد ⇒ **واقف فوق** z=438 عند y=−798، و`Busy` يعود false. الضغط عند y=−339 (أبعد من 250) ⇒ قفزة عاديّة (ذروة z=180).
  - ملاحظة: عيّنة واحدة (الإطار 6) كانت عند y=−791، z=101 — الجسم داخل حجم الجدار لحظةً قبل الصعود (`DisableConstraint`). لم يُفحص بصريّاً.
  - أداة الاختبار: نداء ارتداد في Python (`register_slate_post_tick_callback`) يحقن `IA_Move` و`IA_Jump` بـ
    `EnhancedInputLocalPlayerSubsystem.inject_input_vector_for_action` عند مسافة محدّدة — توقيت حتميّ بدل تأخير MCP (±0.33 ث).

## ٥٤ · العوائق القصيرة لا تُعبَر — جدول VaultMantle أعادته اللقطة

**العَرَض (المستخدم):** اللاعب لا يتعدّى العوائق القصيرة، وجدار 3.5 م لا يُصعد.

**القياس قبل الإصلاح (PIE، `LevelBlock_Traversable11` ارتفاع 128، جري 500، قفز على 150 سم):** قفزة عاديّة، يصطدم ويقف.
**السبب:** لقطة 19:19 (§٥٢) أعادت `DPTS_VaultMantle_SelectionTable` على القرص إلى نسخة `HEAD` (صفّ `DEMOUSE` فقط)، والمحرّر
أعاد تحميله ⇒ **صفّ `L2H` مفقود** ⇒ لا Vault ولا Mantle لشخصيّتنا. (`git checkout -- Content/DPTS_Assets` منّي لاحقاً أكّد ذلك.)
والجدولان الآخران (`ClimbJump` و`Settings`) نجيا لأنّ المحرّر كان يقفلهما.

**الإصلاح:** نُسخ الجدول من `stash@{0}` (unload ⇒ نسخ ⇒ reload). تبيّن أنّ صفّ `L2H` فيه مدخل تجريبيّ `HighMantle_3_5`
(300–400 ⇒ `AM_L2H_Climb_3_5_run_F_Lfoot`) يُسقط اللاعب عبر الأرض (z −851)، و`HighMantle` مقصوص إلى 150–300.
أُعيد الصفّ إلى قيمه الأصليّة الموثّقة في §٤١/§٤٧ بـ`set_data_table_rows` (مفاتيح GUID، dry_run أوّلاً، قراءة عكسيّة):

```
StandardVault   40–180 · 10–50 · ≤350   Hurdle_1_0_run_F_Lfoot
StandardMantle  40–180 · 50–650 · ≤200  Mantle_1_0_run_F_Rfoot
JumpVault       40–180 · 10–50 · ≤350   Hurdle_1_0_run_F_Rfoot (NewEnumerator1)
HighMantle     150–350 · 50–360 · ≤320  Climb_Start_2_5_run_F_Lfoot
```

**القياس بعد الإصلاح (PIE، قفز آليّ بمسافة محدّدة، الزمن ×0.2):**

| العائق | النتيجة |
|---|---|
| 128 سم (`Traversable11`) | `Mantle_1_0_run` ⇒ فوقه ثمّ يتابع |
| 250 سم (`Traversable25`) | `Climb_Start_2_5` ⇒ واقف فوقه (z 337) |
| 350 سم (`Traversable13`) | `WallRunTry` ⇒ `AM_L2H_Climb_2_5_run` ⇒ واقف فوقه (z 438) |

شروط قفزة الجدار 3.5 م: **جري** (سرعة > 250) و**الضغط على بعد ≤ 250 سم** من الوجه؛ أبعد من ذلك قفزة عاديّة.

## ٥٥ · وضعيّة التعلّق حسب وجود جدار تحت اليدين

**قبل:** `ClimbStates` يستعمل كليبات Braced (الجسد مرفوع والقدمان على الجدار) في كلّ مكان، حتى على حافة معلّقة في الهواء.
و`IsBar` كان = `ActiveParkourSubState == 2` — **يتذبذب** 1↔2 كلّ إطارين أثناء التنقّل على جدار (مقاس).

**بعد:**
- دالّة جديدة `BP_L2H_Base ▸ HangHasWall → HasWall`: خطّ من (الممثّل − 120 z) إلى +90 أماماً، `bIgnoreSelf`.
  مقاس: جدار `CORNER_MAIN` ⇒ true · العارضة `Bar_Seg_11` ⇒ false · `LEDGE_TEST` (لوح معلّق) ⇒ false.
- `ABP_L2H_Base ▸ EventGraph`: `IsBar = NOT HangHasWall()` بدل حالة DPTS. أثناء التنقّل على الجدار يبقى false ثابتاً.
- `HangIdle` / `ShimmyRight` / `ShimmyLeft`: `Blend Poses by bool (IsBar)` — true ⇒ `L2H_MD_bar_hang_idle` / `L2H_MD_Bar_Shimmy_R/L`
  (جسد ممدود)، false ⇒ الكليبات Braced كما كانت. الـReach يتبع `IsBar` أصلاً.

## ٥٦ · الزوايا لا تعمل — بحث القفز يسبق بحث الانتقال

**القياس:** عند طرف `CORNER_MAIN` (x 6967، `ClimbValidationFailIndex = 0`) + يمين + قفز ⇒ لا شيء؛ ولا حتى باستدعاء
`SearchForParkourAction` مباشرةً. `InStateActionSearchPriority = Jump` يجد الوجه خلف الزاوية «هدفَ قفزة» فلا يصل إلى
`FindClimbTransitionTarget`.

**الإصلاح (`BP_L2H_Base ▸ HangJump`، أوّل الدالّة):**
```
Branch( FailIndex >= 0  AND  |Right Input| > 0.5 )      ← عند طرف الحافة مع دفع جانبيّ
  then → Priority = Transition → SearchForParkourAction → Priority = Jump
         → Branch(Busy) : true ⇒ انتهى (زاوية) · false ⇒ المسار المعتاد (ProbeScan …)
  else → المسار المعتاد
```

**ارتفاع النهاية:** مُشعِر `height` في المونتاجات الأربعة كان `UseStateWarpOverrides = True`، وتجاوزات الحالة تتبع الحالة
الفرعيّة المتذبذبة (−46/−125 أو −10/−215) ⇒ النهاية 124 سم تحت التعلّق (z 286). صار `False` مع
`Forward −30 · Up −28` (معايَر بثلاث قياسات، الاستجابة 1:1).

**القياس بعد الإصلاح:**

| | النتيجة |
|---|---|
| خارجيّة (`MAIN` شمال ⇒ الوجه الشرقي) | `AM_L2H_Corner_Outer_R` ⇒ x 7031 (31 سم عن الوجه) · z 410 · yaw 180 · يتابع التنقّل |
| داخليّة (`WING` غرب ⇒ `MAIN` جنوب) | `AM_L2H_Corner_Inner_R` ⇒ (6712، −130) — 30 سم · z 410 · yaw 90 |

**ملاحظة:** الإمساك بوجه `MAIN` الجنوبي من الأعلى مختلّ (z 380، 55 سم عن الوجه، `Hang`، لا يتحرّك) — لم يُحقَّق.

**درس أدوات:** بعد عدّة جلسات PIE توجد عدّة `EnhancedInputLocalPlayerSubsystem`؛ اختيار `[-1]` قد يختار القديم فيبدو
اللاعب «عالقاً». يُختار صاحب أكبر `LocalPlayer_N`.

## ٥٧ · تصحيح: معايرة الزوايا كانت على قياس خاطئ (المحرّر يعمل بـ3 إطار/ث)

**ما اشتكى منه المستخدم بعد §٥٦:** عند الانتقال بين الزوايا يتعلّق الجسم «في السماء» بدل أن يبقى على الحافة؛ والتزحلق على الحواف الطويلة سببه تأخّر
بدء الأنيميشن.

**السبب الجذري للشكوى الأولى — خطئي أنا:** المحرّر في الخلفيّة يشغّل PIE بنحو **3 إطار/ث** (دلتا 0.33 ث). مونتاج الإمساك
`AM_L2H_LedgeCatch` يعطي حينها موضع تعليق **z = 410** (اليد 474، فوق الشفة 350 بـ124 سم ⇒ جسد يطفو فوق الحافة). بدلتا صغيرة
(الزمن ×0.06 ≈ 60 إطار/ث) يعطي **z = 276** واليد **341** ⇒ كفّ على الشفة تماماً. قياسات §٥٦ كلّها أُخذت بدلتا كبيرة، فعايرتُ
`Forward −30 · Up −28` ليطابق «410»، وهو وضع خاطئ ⇒ الزوايا صارت تنتهي أعلى بـ~130 سم من التعليق الصحيح، أي معلّقة في الهواء.

**الإصلاح:** أُعيد مُشعِر `height` في المونتاجات الأربعة (`AM_L2H_Corner_*`) إلى `UseStateWarpOverrides = True` بإزاحات 0 (كما كان).
القيم تأتي من `StateWarpOverrides` على المكوّن في `BP_L2H_Base` ((1,1): −24.4 / −162.2 · (1,2): −21.5 / −152.9).

**القياس الصحيح (الزمن ×0.06):** تعليق `LEDGE`: (x 6896، z 276، يد 340، 25 سم عن الوجه) ⇒ زاوية خارجيّة يمين `AM_L2H_Corner_Outer_R` ⇒
**(7022، 57، z 286، يد 349)** — 22 سم عن الوجه الشرقي (x 7001)، اليد على الشفة (350) ⇒ تعليق سليم.

> **قاعدة للاختبارات:** لا تعتمد أيّ قياس ارتفاع/موضع من PIE بزمن 1.0 في هذا المحرّر. شغّل بـ`slomo 0.06` وانقل اللاعب من
> داخل بايثون في الإطار نفسه، وتحقّق أنّ اليد (`hand_r`) على الشفة قبل أيّ معايرة.

## ٥٨ · تأخّر بدء الشيمي: `ClimbSpeed` لا يرتفع إلا بعد بدء الحركة

**السبب:** انتقال `HangIdle ⇒ Shimmy*` كان يشترط `ClimbSpeed > 20` (من §٥١ لإيقاف الانزلاق عند طرف الحافة). السرعة تبقى 0 في
الإطارات الأولى بعد الدفع، ثمّ يضاف تمازج 0.2 ث ⇒ الجسد يتحرّك والأنيميشن لم يبدأ بعد ⇒ «تزحلق».

**الإصلاح (`ABP_L2H_Base`):**
- متغيّر `StallTime` (ثوانٍ): يتراكم بدلتا الزمن ما دام `|ClimbInputRight| > 0.1` **و** `ClimbSpeed < 5`؛ وإلّا يصفّر.
- `HangIdle ⇒ Shimmy*`: العصا فقط + `StallTime < 0.15` (يبدأ الأنيميشن مع الدفع مباشرةً).
- `Shimmy* ⇒ HangIdle`: `|ClimbInputRight| < 0.1` أو `StallTime ≥ 0.15` (عند طرف الحافة يتوقّف خلال 0.15 ث، ولا يعود حتى يُرفع الإصبع).
- شرط الـReach الجانبيّ يستعمل «يتشيمي الآن» = `|Input| > 0.1 AND StallTime < 0.15` بدل `ClimbSpeed > 20`.

## ٥٩ · الكاميرا تدخل في اللاعب أثناء صعود الجدار 3.5 م

**القياس (PIE بزمن ×0.06، جري ثمّ ضغط القفز على 230 سم من الوجه):** المسافة بين الكاميرا واللاعب ≈ 240 سم أثناء الجري،
ثمّ **تنهار إلى 65 سم خلال 3 إطارات** (241 → 138 → 81 → 66) لحظة يبلغ اللاعب الوجه، وتبقى 62–66 حتى قمّة الجدار ثمّ تعود (≈ 200).

**السبب:** منصّة الكاميرا `GameplayCameraComponent` (`CameraAsset_SandboxCharacter`) فيها عقدة `CollisionPushCameraNode`
(`CameraRig_CollisionOffset`): «نقطة الأمان» = الشخصيّة + (0، 20، 150)، قناة `ECC_Camera`، كرة 10 سم. أثناء الصعود يُنقَل
الكبسولة داخل الجدار (تحذير حركة DPTS بلا تصادم) فتصير نقطة الأمان **داخل المجسّم** ⇒ تُسحب الكاميرا إلى نقطة الأمان ⇒ تدخل في اللاعب.

**التحقّق:** جعل `LevelBlock_Traversable13` يتجاهل `ECC_Camera` (تجربة مؤقّتة داخل PIE) ⇒ المسافة بقيت 219–241 طوال الصعود.

**الإصلاح (في `BP_L2H_Base` فقط، بلا مساس بأصول الكاميرا):**
- `WallRunTry`: قبل `TriggerActionMontage` ⇒ `SetCollisionResponseToChannel(HitComponent, ECC_Camera, Ignore)` + حفظ المكوّن في `CameraIgnoredComp`
  + `CameraIgnoreOn = true`.
- `UpdateMovement_PreCMC` (أوّل الدالّة): `Branch(CameraIgnoreOn AND NOT DPTS.Busy)` ⇒ `SetCollisionResponseToChannel(CameraIgnoredComp, ECC_Camera, Block)` ⇒ `CameraIgnoreOn = false`.
  أي يُعاد الحجب بمجرّد انتهاء الفعل.
- لم يُطبَّق على الـVault/Mantle/HighMantle العاديّة (لم تُلاحَظ المشكلة فيها)؛ نفس الآليّة قابلة للتعميم إن ظهرت.

**تصحيح على ما سبق (كاميرا):** أوّل محاولة ضبطتُ فيها استجابة `HitComponent` من **الخطّ الأوّل** (يبدأ على z+48 فيصيب **الدرجة السفليّة** `C_10`
ارتفاعها 100 سم)، بينما الجدار المتسلَّق هو `C_12` (z 100–350) وتصيبه **الخطّ الثاني** (النازل). وتجربتي اليدويّة الأولى كانت على `C_12`
(الوسم `LevelBlock_Traversable13`) فنجحت، فلمّا نقلتُها إلى BP لم تؤثّر. الإصلاح الصحيح: `K2Node_CallFunction_40 ▸ HitComponent` (الخطّ الثاني).

**القياس النهائي (قفز فعليّ، ×0.06):** مسافة الكاميرا أثناء الصعود **215 – 270 سم** (كانت تنهار إلى 64). والمكوّن يعود `Block` بعد انتهاء الفعل (مقاس: `ECR_BLOCK`، `CameraIgnoreOn=false`).
**درس:** قراءة خاصيّة عبر وسم الممثّل في PIE قد تعيد ممثّلاً آخر باسم مختلف (`LevelBlock_Traversable_C_12` ≠ `_C_10`)؛ اقرأ المكوّن من المتغيّر نفسه لا من الوسم.

## ٦٠ · الغوص في الجدار أثناء صعود 3.5 م — مقاس، ولا إصلاح بلا ثمن

**القياس (×0.06، `LevelBlock_Traversable` 350، وجه عند d=700):** مركز الكبسولة يمشي من d=659 (41 سم عن الوجه) عند بدء نافذة الـwarp
الأولى (t=0.38) إلى **d=694** عند t=0.68 (6 سم من الوجه، أي الكبسولة نصف قطرها 34 داخل الجدار بـ28 سم) بينما z لم يرتفع بعد (124)،
ثمّ يصعد (z 275 عند 0.92) ويعود إلى d=675. الاندفاع داخل الجدار جزء من كليب GASP (`Climb_2_5`) بعد تعطيل التصادم.
**محاولات:** (١) إزاحة `HeightTarget` للخارج 20 ثمّ 80 سم ⇒ الغوص يقلّ 7 ثمّ 12 سم فقط، لكنّ **هدف الهبوط `AN_FindLandTarget` يتبع الهدف**
فينتهي الصعود في الهواء أمام الجدار (d=645 عند z 350). (٢) `ForwardOffset` الإشعار الأوّل −50 ⇒ −80: نفس النتيجة (الصعود يطفو 30 سم أمام الجدار).
كلاهما أُعيد (−50، الهدف عند 25 سم داخل الوجه). **الباقي:** الغوص ≈ 28 سم لنحو 0.3 ث — حلّه الصحيح بناء مونتاج بثلاثة warp
(اقتراب · تعلّق على الحافة · اعتلاء) بدل تكييف مونتاج GASP.
**أُضيف أيضاً:** `CameraIgnoredComp`/`CameraIgnoreOn` (§٥٩) بقيت. وعُدِّل `ClimbDownTry` فقط.

## ٦١ · النزول من فوق الحافة إلى التعلّق (`ClimbDownTry`)

**الأصول:** `L2H_MD_climb_down` و`L2H_MD_bar_climb_down` (إعادة استهداف `RTG_ALS_to_UEFN` من ATS)، ومونتاج `AM_L2H_ClimbDown` (1.33 ث):
نافذة warp `height` (0 → 1.1 ث) + `AN_ResetParkourAction` عند 1.15. **أخطاء وقعتُ فيها وأصلحتها:**
- طول المونتاج المُنشأ بالأداة 0 حتى يُحمَّل من جديد (احفظ ثمّ `unload_packages`/أعد التحميل).
- مُشعِر الـwarp الجديد اسم هدفه `None` و`IgnoreZ=True` ⇒ لا يعمل (الموضع من حركة الجذر فقط). صُحّح بـ`root_motion_modifier.warp_target_name='height'` و`ignore_z_axis=False` (من بايثون).
- مُشعِر `AN_ResetParkourAction` افتراضيّاً `ActivateState=False` ⇒ لا تُثبَّت الحالة فيسقط اللاعب. صار `ActivateState=True` و`BlendTime=0.2` (كالتقاط الحافة).
- اتّجاه الهدف: `OrientationUse = PrimaryTargetNormal (1)` مع `HeightTargetNormal` = ‏الناظم الخارجيّ للوجه (بلا قلب).

**الدالّة `BP_L2H_Base ▸ ClimbDownTry`** (تُستدعى من `IA_Crouch ▸ Started` عند حالة DPTS = None):
شروط: على الأرض · سرعة < 100 · `DPTS.ActiveParkourState == None` · خطّ أفقيّ من (موضع + 130 أماماً) إلى (+40) على z−128 يصيب وجهاً
(`ImpactNormal·Forward > 0.6`، ليس تداخلاً ابتدائيّاً) · خطّ ثانٍ على z−208 يصيب الوجه (جدار ≥ 120 سم) · الممثّل موسوم `climb`.
تنفيذ: `HeightTarget = (وجه.x, وجه.y, z_القدمين)` · `HeightTargetNormal` · `Busy=true` · `TempParkourState=LedgeClimb` · `TempParkourSubState=FullContact`
⇒ `TriggerActionMontage([AM_L2H_ClimbDown])`. إن صار `Busy` ⇒ `SkipCrouchToggle=true` (لا يجلس).

**القياس (×0.06، `LEDGE_01` من (3640، 0، z 438) متّجهاً إلى الشرق، وجهها x=3700):** المونتاج ⇒ **(3726، z 276، yaw −180)**، `ActiveParkourState=1`، `Busy=false`،
اليد z=341 على الشفة (350) — مطابق تماماً للإمساك بالالتقاط. والجلوس في منتصف الحافة (x=3000): `Busy=false` وجلس فعلاً.
**لم يُنجَز:** نسخة العارضة/الحافة المعلّقة (`L2H_MD_bar_climb_down` مُعاد استهدافها فقط)، ولا اختبار الاتّجاهات الأخرى (يسار/يمين/الجدران غير الشرقيّة).

## ٦٢ · القفز من الحافة بالاتجاهات القطرية (`ProbeSelect`) + الجلوس = قفزة للخلف

**الجلوس أثناء التعلّق** صار قفزة للخلف (`Disengage` ثم `LaunchCharacter(-400·Fwd + (0,0,350))` + التفاف) بدل مونتاج الإفلات؛ مُختبَر بالقراءة (مسار الحدث مشترك لكل حالات التسلّق). لم يُقَس في PIE لأنّ PIE كان متوقّفاً عن التحديث (نافذة المحرّر في الخلفية).

**العلّة (مقاسة):** فلتر المرشّحين في `ProbeSelect` يقبل «نصف مستوى بلا مخروط» لأيّ مدخل مركّبته الرأسية |sin| ≥ 0.5، والقطري sin=0.707 يستوفيه ⇒
الحافة الجانبية الأقرب تفوز على الحافة القطرية الحقيقية (الدرجة = المسافة/100 + 3(1−cos)).
**الاختبار:** جسم تعلّق (نزول من فوق منصّة x=20000)، و7 كتل `climb` عند (R,U) = (0,110) (±130,110) (±130,0) (±130,−110) + كتلة بارزة تحت، ثمّ `ProbeScan`+`ProbeSelect` بكل الاتجاهات الثمانية:
| مدخل | قبل | بعد |
|---|---|---|
| U | (y20002, z510) ✓ | ✓ |
| UR | (19925, 400) ✗ جانبية | (19875, 510) ✓ |
| R | (19925, 400) | (19925, 400) |
| DR | (19925, 400) ✗ | (19875, 290) ✓ |
| D | (20125, 290) | (20125, 290) |
| DL | (20075, 400) ✗ | (20125, 290) ✓ |
| L | (20075, 400) | (20075, 400) |
| UL | (20075, 400) ✗ | (20125, 510) ✓ |
(R/L يقعان على هدف مدموج لحافة المنصّة وكتلة الجانب — الدمج 60 سم من `ProbeSpan`، وليس خطأً في الاتّجاه.)

**الإصلاح (`ProbeSelect` فقط):** عتبة «نصف المستوى» 0.5 → 0.9 (للأعلى/الأسفل الصرفين)، وعتبة المخروط `0.5` ← `0.5 + 0.6·sin²` (0.5 جانبيّ، 0.8 ≈ ±37° قطريّ، 1.1 رأسيّ فيعتمد نصف المستوى). عقد جديدة: `CCA_10/11/12`؛ العقدة `CallFunction_11` (الحرفيّة 0.5) صارت يتيمة.
**لم يُحلّ/لم يُختبَر:** كتلة «D» البارزة تحت اللاعب لم تُكتشَف أبداً (مرشّح غائب من `ProbeScan`، ربّما لأنّ حزم الفحص أفقية من وجه الحائط)؛ ولم أُجرِ القفزة الفعليّة بالعصا القطرية (القياس عند الاختيار فقط، لا عند `CalculateClimbJump`).

## ٦٣ · الغوص في جدار 3.5 م — حُلّ بنافذتَي warp بينهما فجوة (يحلّ §٦٠)

**القياس الصحيح (اختراق العظام لا الكبسولة):** لكلّ إطار، أقصى عمق لعظام اليدين/القدمين/الرأس/الحوض/الساقين خلف وجه
`Traversable13` (y=−700) ما دامت بين z 100 و347. الأصل: **اليد 53 سم** (0.63 ث) و**القدم 66 سم** (0.70 ث).
**السبب:** جذر الكليب (`M_L2H_Climb_2_5_run_F_Lfoot`) يتقدّم أماماً بلا رجوع ولا ارتفاع حتى 0.6 ث؛ نافذة warp واحدة (0.36→0.97)
تحوّل (178 أماماً، 103 أعلى) إلى (قليل أماماً، 188 أعلى) بقصّ خطّيّ ⇒ الحركة الأماميّة المبكّرة تدخل الجدار ثمّ ترجع.
والكليب «يتوقّع» الجدار على 40–63 سم أمام الجذر وقت وضع اليدين/القدم، لا 25.
**فخّ مكتشَف:** نافذتان متلاصقتان بنفس الهدف `height` ⇒ قفزة لحظيّة (z 148→280 في إطار) لأنّ `AN_MotionWarp_WithOffset::NotifyBegin`
يستدعي `AddOrUpdateWarpTarget` فيرى المعدِّل السابق — في آخر جزء من ثانيته — الهدفَ يقفز فيلحقه. **الحلّ: فجوة 0.025 ث بين النافذتين.**
**الإعداد النهائي (`AM_L2H_Climb_2_5_run_F_Lfoot`):**
| نافذة | الزمن | الهدف | Forward / Up |
|---|---|---|---|
| ١ اقتراب | 0.364 → 0.675 | `height` | −70 / −273 (≈45 سم عن الوجه عند الدفع) |
| ٢ إمساك الشفة | 0.700 → 0.966 | `height` | −50 / −160 (كما كان — نفس نقطة التعلّق) |
| ٣ اعتلاء | 1.428 → 1.974 | `land` | 0 / 0 (لم يُمسّ) |
**النتيجة (×0.03 بعد القفز):** اليدان ≤ 9 سم · القدم اليمنى ذروة 34 سم (0.76 ث، لحظة الدفع) · ارتفاع متّصل (156→172→187→205…) · ينتهي واقفاً (y −792، z 438).
جُرّبت نسخة رابعة (نافذة ١ حتى 0.80): أسوأ (اليد 33) فأُعيدت الثالثة.
**درس قياس:** المحرّر في الخلفية لا يتقدّم إلّا مع استدعاءات MCP أحياناً؛ و`unload_packages` لا يفرّغ أصلاً مُشاراً إليه — `reload_packages` بعد تبديل الملف.

## ٦٤ · الالتفات 180° في القفز للخلف (`BackLeap`)

**قبل:** القفز للخلف (الجلوس أثناء التعلّق، أو أسفل+قفز بلا حافة) = `LaunchCharacter(−400·Fwd + 350·Z)` + تدوير فوريّ للممثّل ⇒ لا أنيميشن التفات.
**الآن:** دالّة `BP_L2H_Base ▸ BackLeap`: `HangHasWall` ⇒ `LaunchCharacter((0,0,350), XY=override)` ⇒ `PlayAnimMontage(AM_L2H_LeapBack | AM_L2H_BarLeapBack)`.
المونتاجان من ATS (`MD_leap_short_l_back`، `MD_bar_leap_short_l_back`، إعادة استهداف + تفعيل حركة الجذر): الجذر يرجع 240 سم ويستدير 180° خلال 0.8 ث وZ=0،
والمحرّك يتجاهل Z حركة الجذر أثناء السقوط ⇒ الأفقيّ والالتفات من الكليب، والعموديّ من الدفعة والجاذبيّة. `BlendOutTriggerTime` = (الطول − 0.85) لكي
لا تبقى وضعيّة التعلّق في ذيل الكليب بعد الهبوط. يستدعيها مساران: `EventGraph ▸ IA_Crouch (Started) ▸ حالات التسلّق` بعد `DisengageFromState`،
و`HangJump ▸ ProbeResult=3 ▸ DirU ≤ 0` (بدلاً من `Launch`+`SetActorRotation` المحذوفتين).
**القياس (×0.3):** من التعلّق على (20024، z 326، yaw −180) ⇒ المونتاج 0.2→0.8 ث، yaw −180 → 90 → 0، ينتقل 247 سم للخلف ويهبط (z 88)؛
المونتاج انتهى لحظة الهبوط. إن وُجدت حافة خلفه ضمن المدى فالالتقاط الهوائيّ القائم يمسكها (لم يُختبر هنا).

## ٦٥ · النزول إلى تعلّق حرّ (عارضة/لوح بلا جدار) — `ClimbDownTry`

**إضافة (بلا مساس بمسار الجدار):** حين يفشل شرط الجدار، خطّ ثالث من (موضع + 130 أماماً) إلى (+40) على **z القدمين − 10** يصيب حافة لوح
رقيق. الشرط: يصيب · ليس تداخلاً ابتدائيّاً · `ImpactNormal·Forward > 0.6` · الممثّل موسوم `climb` · **والخطّ الثاني (z−208) لا يصيب**
(لا جدار تحت الحافة ⇒ تعلّق حرّ). التنفيذ: `HeightTarget`/`Normal` من هذا الخطّ، `TempParkourSubState = Hang (2)`،
`TriggerActionMontage(BarClimbDownMontages)` (متغيّر جديد = `[AM_L2H_BarClimbDown]`).
**المونتاج:** `AM_L2H_BarClimbDown` من `L2H_MD_bar_climb_down` بنفس إشعارات `AM_L2H_ClimbDown` (warp `height` 0→1.1 بـ `OrientationUse=1`،
Reset عند 1.15 بـ `ActivateState=True`). **ملاحظة:** `clone_notify_setup` أفسد النسخ (وضع Reset عند 0 وتخطّى الـwarp) ⇒ أُضيفا يدويّاً،
و`BlendTime` في Reset كان 0 فصار 0.2.
**القياس:** `LEDGE_TEST` (لوح 40 سم، قمّته 400، الطرف x=1100) من (1040، z 488) + جلوس ⇒ الحالة 1، الفرعيّة 2 (Hang)، (1122، z 335)،
اليد z=400 على الشفة تماماً. ومسار الجدار بعد التعديل: `LEDGE_01` ⇒ (3726، z 276، yaw −180) كما في §٦١.

## ٦٦ · Reach In/Out — حالتا دخول وخروج لمدّ اليد

**الأصول:** 40 كليباً من ATS (`Reach/Braced/01/In|Out` و`Reach/Freehang/In|Out` — بلا «back»)، إعادة استهداف `RTG_ALS_to_UEFN` ⇒ `L2H_MD_[bar_]reach_long_{in|out}_{l|r}_*`،
و8 BlendSpaces نُسخت من `L2H_Reach_[Bar_]{L|R}_BS` مع تبديل العيّنات 3–7 (`L2H_Reach_[Bar_]{In|Out}_{L|R}_BS`).
**`ABP_L2H_Base ▸ ClimbStates`:** حالات جديدة `ReachIn{Left|Right}` و`ReachOut{Left|Right}` — كلّ منها BS جدار + BS عارضة ⇒ `BlendListByBool(IsBar)`،
X ← `ReachAngle`، Y = 100 ثابت، بلا تكرار. الانتقالات:
`HangIdle|ShimmyL|ShimmyR → ReachIn*` (نفس شرط الدخول القديم) · `ReachIn* → Reach*` عند `ReachDistance > 95` · `ReachIn*|Reach* → ReachOut*` عند `ReachTarget < 50`
· `ReachOut* → HangIdle` عند `ReachDistance < 5` · `ReachOut* → ReachIn*` إن عاد الهدف. (`ReachDistance` = FInterp سرعة 8 ⇒ ≈ 0.37 ث، بطول كليبات In/Out 0.4 ث.)
**فخّان:** (١) القاعدة «التلقائيّة» (وقت متبقٍّ للمشغّل) لا تعمل مع BS خلف `BlendListByBool` ⇒ علِق في `ReachInRight`. (٢) `set_transition_rule` بقاعدة رقميّة
لا يمسح علَم «التلقائيّة» ⇒ بقيت عالقة؛ الحلّ حذف الانتقال وإعادة إنشائه ثمّ القاعدة.
**القياس (PIE، جسم الاختبار، عصا يميناً 2.0→3.2 ث):** `HangIdle → ReachInRight` (Distance 10) → `ReachRight` (95.8) → بعد الإفلات ومهلة الإمساك
`ReachOutRight` (Target 0، Distance 36) → `HangIdle` (2.2). الجهة اليسرى بنفس البنية (مقروءة) — لم تُقَس منفصلة.
**حادث:** محاولة قراءة اسم الحالة بـ`call_method('GetCurrentStateName', i)` من بايثون أسقطت المحرّر (رقم الآلة الحقيقيّ 99)؛ لا خسارة (كلّ شيء محفوظ).
بعد إعادة التشغيل اختار أحدهم «Restore Packages» فكُتبت `DefaultLevel.umap` من الحفظ التلقائيّ (أسماء الممثّلين مطابقة لـHEAD؛ لم تُودَع ولم تُعَد — قرار المستخدم).

## ٦٧ · المشي على العارضة (Beam)

**التصميم:** DPTS لا يملك أنيميشن عارضة، ونظام ATS للعارضة نظام منفصل بشجرة عظام أخرى ⇒ بناء خاصّ بنا في `BP_L2H_Base` و`ABP_L2H_Base`.
**الكشف — `BeamUpdate`** (يُستدعى كلّ إطار في `UpdateMovement_PreCMC` قبل `Set MaxWalkSpeed`): خطّ نازل 120 سم من مركز الكبسولة؛ إن كان
على الأرض والممثّل موسوماً `beam` ⇒ محور العارضة = المحور الأفقيّ الأطول في `GetActorBounds` · `SetPlaneConstraintNormal/Origin/Enabled`
(قيد على مستوى المحور الطوليّ) · لصق الموضع على خطّ المنتصف · `IsOnBeam = true` · `BeamNormal`. وإلّا وكان على عارضة ⇒ فكّ القيد و`IsOnBeam = false`.
السرعة: `MaxWalkSpeed = IsOnBeam ? min(GASP, 200) : GASP`.
**الاتّجاه:** القيد وحده لا يكفي — `DPTS ▸ ParkourInputOverride` (بائع) يستدعي `AddMovementInput` بالمدخل الخام ⇒ الجسم يدور نحو العصا (31°).
الحلّ قبله: دالّة `BeamProjectDir(Dir)` (تحذف مركّبة `BeamNormal` إن كان على عارضة) ⇒ يُعاد تحويل المدخل إلى (يمين، أمام) ويُمرَّر إلى `ParkourInputOverride`،
وكذلك قبل `AddMovementInput` في مسار GASP. خارج العارضة تُعيد المدخل كما هو (لا أثر على التسلّق).
**الأنيميشن:** 4 كليبات ATS (`MD_beamwalk_idle/walk_slow/walk/run` — ثابتة في المكان، سرعاتها 0/80/160/320) ⇒ `L2H_BeamWalk_BS` (1D على `Speed2D`)؛
`ABP ▸ AnimGraph`: `BlendListByBool(IsOnBeam)` جديد بين سلسلة GASP (`PoseSearchHistoryCollector`) ومدخل «غير متسلّق» في `BlendListByBool(IsClimbing)`.
**القياس (عارضة 40×600 سم موسومة `beam`):** عصا قطريّة ⇒ تسارع (800، 0)، yaw 0، y ثابت 20000، سرعة 200؛ عصا جانبيّة صرفة ⇒ لا حركة؛ للخلف ⇒ yaw 180؛
عند الطرف (x≈21600) يسقط، `IsOnBeam=false` في الشخصيّة والـABP، ثمّ GASP (500).
**حدود:** العارضة يجب أن تكون محاذية لمحور X أو Y (المحور من AABB العالميّ)؛ لا أنيميشن توازن/سقوط جانبيّ (الكليبات `balance_l/r` و`fall_down` غير مستعملة)؛
مسار `IA_Move_WorldSpace` (وضع العصا المزدوجة) لم يُسقَط؛ والنزول للتعلّق من العارضة يعمل عبر §٦٥ إن وُسمت أيضاً `climb` وكانت معلّقة.

## ٦٨ · لا تنقّل ولا قفز بعد النزول — صفّ `L2H` ضاع من جدول الإعدادات

**العَرَض:** بعد النزول من السطح إلى التعلّق (§٦٠) لا ينتقل يميناً/يساراً ولا يقفز. `Right Input` يصل (±1) لكن `ClimbValidationFailIndex = 0`
و`MovementMode = None` من أوّل إطار فيه مدخل — على جدار نظيف 20 م في جسم الاختبار وعلى `LEDGE_02` في المستوى.
**ما استُبعد بالقياس:** المسافة عن الوجه (24 و49 سم ⇒ نفس الفشل) · قناة التتبّع (الجدار يحجب Visibility) · `TransformSave` (= تحويل الممثّل) ·
`OwnerRef` · تصادم الشخصيّة (يتجاهل Visibility) · تكرار خطوط `ValidateClimbArea` الثلاثة من بايثون بالقيم الحيّة ⇒ كلّها تصيب.
**الدليل الحاسم:** دالّة مؤقّتة في `BP_L2H_Base` تستدعي `DPTS_BFL ▸ ValidateClimbArea` — بإعدادات الجدول مكتوبة يدويّاً ⇒ نجاح (−1)؛
بإعدادات `GetClimbValidationSettings` الحيّة ⇒ فشل (0) و`PrimaryValidationTraceDistance = 0`، `CharacterSpaceValidationSize = 0`.
**السبب:** `BP_L2H_Base ▸ DynamicParkourTraceSystem ▸ SettingsUseIndex = 1` (§٤٠) بينما الجدول فيه صفّ واحد فقط (`NewRow`).
`SetSystemSettings`: `GetDataTableRowNames[1]` خارج الحدود ⇒ `RowNotFound` ⇒ `CurrentSettingsListing` يبقى افتراضيّاً ⇒ **كلّ** إعدادات التسلّق صفر
(تحقّق، سرعة، بحث القفز). صفّ `L2H` (§٤٠، §٤١، §٤٣) لم يُودَع قطّ وضاع في محو «Teleport auto-stash» (2026-10-01 19:19)؛ الشخصيّة بقيت في الذاكرة فاحتفظت بالفهرس 1.
**العلاج:** الجدول من `stash@{0}` (`f0d223b3`، 28720 بايت، الصفّان `NewRow`, `L2H`) عبر unload ← نسخ ← `reload_packages`.
مقارنة آليّة بعده: `NewRow` مطابق لـHEAD حرفيّاً؛ `L2H` يختلف بما وثّقته §٤١/§٤٣ فقط (`LedgeCatch` 160/130، `JumpFromDropOff` 200 / +130 / Boost 0).
الدالّة المؤقّتة حُذفت.
**القياس (PIE، `LEDGE_02` x=4300):** نزول ⇒ تعلّق (FullContact) · يمين 1.2 ث ⇒ x 4300→4195 (≈90 سم/ث = `MaxSpeed`) · يسار ⇒ يعود · `FailIndex = −1` طوال الوقت ·
قفز + عصا أعلى ⇒ `ProbeResult = 2` ⇒ صعود والوقوف على السطح (z 438).
**ملاحظة اختبار:** النقل الآني لا يُنهي حالة DPTS؛ اختبار يبدأ والشخصيّة ما زالت متعلّقة يجعل الجلوس قفزة للخلف (§٦٤) — يبدو كفشل نزول متقطّع وليس كذلك.

### ٦٨-ب · جرد كامل لـ`stash@{0}` مقابل الشجرة الحاليّة

مقارنة كلّ ملفّ في اللقطة (966) بالشجرة بالـhash: 74 مطابق، 886 «ناقص» — كلّها حزم بيئة (`Laboratory`، `AbandonedPowerPlant`،
`Modular_Scifi_Mechanic_Base`) خارج نطاق الحافة، **لا شيء من `_Project` ناقص**. المختلف 6:
- `DPTS_Settings_SelectionTable` — أُصلح أعلاه.
- `DPTS_ClimbJump_SelectionTable` — صفّ `L2H` كان **8** عناصر (الأربعة × قصير/طويل) بدل **28** الموثّقة (16 للجدار بالأقطار + 12 للعارضة `BarJump_*`).
  الاسترجاع بنفس الطريقة؛ JSON بعده = JSON اللقطة حرفيّاً. القياس: نزول على منصّة الاختبار ⇒ عصا أعلى-يسار + قفز ⇒
  `AM_L2H_ClimbJump_UpLeft_Long` يُشغَّل ويصل إلى العارضة (20023، 20149، z 462). قبله لم يكن في الجدول أيّ مونتاج قطريّ.
- `DPTS_VaultMantle_SelectionTable` — المدخل الناقص `HighMantle_3_5` حُذف **عمداً** في §٥٣ (يُسقط عبر الأرض) ⇒ الحاليّ صحيح.
- `ABP_L2H_Base`، `BP_L2H_Base`، السجلّ، `ASSET_LICENSES.md` — تطوّرت بعد اللقطة بإيداعات لاحقة.
**الدرس:** جملة §٥٣ «الجدولان الآخران نجيا لأنّ المحرّر كان يقفلهما» كانت خاطئة — الملفّ المقفل نجا على القرص، ثمّ أعادته «استعادة HEAD» لاحقاً.

## ٦٩ · إقفال ملفّ الحافة — آخر القياسات وما بقي خارج النطاق

**Reach الجهة اليسرى (PIE، منصّة الاختبار بعد النزول، عصا يساراً):** `HangIdle → ReachLeft` (مأخوذة من `sample_pie_anim_instance`:
`ReachTarget 100`، `ReachDistance 100`، `ReachAngle −90`)؛ بعد الإفلات `ReachDistance` 100 → 33 → 1 → 0 ثمّ `HangIdle`.
حالة `ReachOutLeft` نفسها (≈0.4 ث) وقعت بين عيّنتين — مرّت قيم شرطها، ولم تُلتقط باسمها.

**اللاعب الثاني (شاشة مقسومة):** فحص ثابت — لا `GetPlayerController/Character/Pawn` في `BP_L2H_Base` ولا DPTS؛ الوحيد في
`ABP ▸ Debug_ExperimentalStateMachine` (دالّة تصحيح من GASP خارج مسار الحافة). فحص حيّ — `GameplayStatics.CreatePlayer` ⇒ `PC_Sandbox_C_1`
+ `BP_L2H_Base_C_1`؛ إدخال من نظامه الفرعيّ فقط (`LocalPlayer_30`): نزول ⇒ تعلّق ⇒ يمين ⇒ x 4300→4210، `FailIndex −1`؛
اللاعب الأوّل لم يتحرّك (`Right Input = 0`). **ملاحظة خارج النطاق:** تحذير عند الإنشاء —
سياق `GameplayCamera` للشخصيّة الثانية أُنشئ أوّلاً لـ`PC_Sandbox_C_0` ثمّ أُعيد إنشاؤه؛ يستحقّ فحصاً في `SetupCamera` عند بناء الشاشة المقسومة.

**التنظيف:** حُذفت ممثّلات `ZZ_PROBE_RIG` (11) من عالم المحرّر؛ المستوى لم يُحفظ.

**باقٍ وموثّق (لا يُصلح هنا):**
- `DefaultLevel.umap` معدَّل على القرص منذ «Restore Packages» (§٦٦) — أسماء الممثّلين مطابقة لـHEAD؛ القرار للمستخدم (إيداع أو `git restore`).
- `.hyperai/Start-HyperAIStudioEditor.ps1` يمرّر مسار المشروع بلا علامات اقتباس ⇒ مع المسافة في `Epic Games` يفتح مشروعاً آخر.
- العارضة (§٦٧): محاذية للمحاور فقط، بلا توازن/سقوط جانبيّ، ومسار `IA_Move_WorldSpace` غير مُسقَط.
- حزم البيئة في `stash@{0}` (`Laboratory`، `AbandonedPowerPlant`، `Modular_Scifi_Mechanic_Base`) ليست في الشجرة — خارج نطاق الحافة.

## ٧٠ · الزوايا تلقائيّاً بالاتجاه وحده + الـReach لا يغطّي الحركة

**بلاغ المستخدم:** «ReachLeft لا يعمل — التنقّل عند زاوية الحافة لا يعمل — عند توجيه Reach يكتب فوق الحركة فيبدو اللاعب يتزحلق».

**القياس قبل التعديل (PIE، `CORNER_MAIN` الوجه −Y، زاوية داخليّة x=6800 مع `CORNER_WING` وخارجيّة x=6199):**
- الزاوية تعمل **فقط** بقفزة والجسم واقف (`FailIndex ≥ 0`): داخليّة ⇒ `AM_L2H_Corner_Inner_L`، خارجيّة ⇒ `AM_L2H_Corner_Outer_R`.
  القفز أثناء الحركة (`FailIndex −1`) يذهب للمسبار فيقفز 44 سم على **نفس** الحافة بمونتاج `BarJump_Left` بدل الزاوية.
- الانزلاق: شرط إخفاء الـReach أثناء الحركة كان للهدف الجانبيّ الصرف فقط (`|ProbeDirU| < 0.5`)؛ بعصا قطريّة يظهر الـReach والجسم يتحرّك.
- العصا القطريّة توقف الجسم أقرب إلى الطرف: `ValidationOffset = RightInput × 40` ⇒ 0.7 × 40 = 28 ⇒ يقف عند 17 سم من الحافة
  (6216) بدل 26 (6225)، و`CornerTargetDistance = 30` ⇒ بحث الزاوية يفشل من هناك.
- ReachLeft نفسه **يعمل** (`ClimbStates = ReachLeft`، الزاوية −90): المقاطع والـBS متناظرة يميناً/يساراً (مقاسة بالعظام والعيّنات).
  ملاحظة: وضعيّة ATS «reach long side» تُسقط اليد الحرّة 60 سم وتبعدها 65 سم عن الجدار في الجهتين.

**التعديلات:**
- `ABP_L2H_Base ▸ EventGraph`: الإخفاء أثناء الحركة صار لكلّ الاتّجاهات: `active AND NOT( |ClimbInputRight|>0.1 AND StallTime<0.15 )`
  (حُذفت عقد `|ProbeDirU|<0.5` و`ClimbSpeed>20` اليتيمة).
- `BP_L2H_Base ▸ CornerAutoTry` (جديدة، تُستدعى كلّ إطار بعد `BeamUpdate` في `UpdateMovement_PreCMC`؛ متغيّر `CornerStall`):
  إن `ActiveParkourState ≠ None AND FailIndex ≥ 0 AND |Right Input| > 0.5 AND NOT Busy` تتراكم المدّة؛ بعد 0.25 ث ⇒ `CornerStall = −1` (مهلة 1.25 ث)،
  `InStateActionSearchPriority = TransitionAction` ⇒ `ParkourInputOverride(sign R, 0)` ⇒ `UpdateParkourStateData(sign R, 0)` ⇒ `SearchForParkourAction` ⇒ `JumpAction`.
  (`UpdateParkourStateData` لازم: `FindClimbTransitionTarget` يقرأ متغيّرَي DPTS `Right/Forward Input`، و`ParkourInputOverride` لا يكتبهما.)
- `BP_L2H_Base ▸ EventGraph`: المدخل الجانبيّ إلى `UpdateParkourStateData` يُقرَّب إلى ±1 إن `|R| > 0.3` ⇒ نقطة التوقّف لا تتعلّق بزاوية العصا.
  `ProbeOnInput` ما زال يأخذ القيمة الأصليّة (اتّجاه القفز القطريّ لم يتغيّر).

**القياس بعد (PIE، بلا قفز):**
- داخليّة، عصا يساراً: 6500 → 6755 ⇒ `Corner_Inner_L` تلقائيّاً ⇒ يكمل على وجه `CORNER_WING` (y −155 → −335).
- خارجيّة، عصا **قطريّة** أعلى-يمين: 6300 → 6226 بـ`ReachTarget = 0` طوال الحركة ⇒ توقّف ⇒ `Corner_Outer_R` بعد ≈0.3 ث ⇒ على الوجه −X
  (y −57 → 68) ⇒ زاوية خارجيّة ثانية ⇒ الوجه +Y. أي دوران كامل حول الكتلة بالعصا وحدها.
- القفز عند الزاوية ما زال يعمل كما كان (نفس مسار `HangJump`).

**باقٍ:** القفز الجانبيّ أثناء الحركة على حافة متّصلة ما زال يقفز على الحافة نفسها (المسبار يقبلها هدفاً)؛ وبعد الزاوية الداخليّة يظهر الـReach
لعيّنة واحدة (≈0.35 ث من `ReachHold`).

## ٧١ · القفز والـReach يتبعان الهدف لا العصا + لا قفزة على نفس الحافة

**بلاغ المستخدم:** «يسار أحياناً ينظر وأحياناً لا — القفز لليسار أحياناً يشبه اليمين وأحياناً صحيح».

**التشخيص (جسم الاختبار: منصّة + عوارض ±130 سم بثلاثة ارتفاعات):**
- المونتاجات سليمة: كلّ `AM_L2H_*Jump_*` يشغّل مقطعه الصحيح (مقروء من segments)، ومقاطع اليسار مرآة اليمين بالضبط (جذر +X/−X بنفس المسافات).
- عصا يسار/يمين صرفة (ومع ارتخاء قبل القفز) ⇒ صحيح في الجهتين.
- **العطل:** عصا بزاوية ≈30° (−0.9, 0.5) ⇒ المسبار يختار العارضة **الأفقيّة** (أقرب)، لكنّ المونتاج `ClimbJump_UpLeft` وينتهي عند z 357 بدل 332 —
  مونتاج قطريّ يُمطّ إلى هدف أفقيّ. ومتناظر يميناً (`UpRight`). السبب: `HangJump` يمرّر اتّجاه الهدف إلى `ParkourInputOverride`
  الذي **لا يكتب** متغيّرَي DPTS `Right/Forward Input`؛ و`CalculateClimbJump` يختار المونتاج منهما ⇒ يختار من العصا لا من الهدف.
- الـReach: الزاوية من `ProbeDirR/U` (العصا) لا من الهدف ⇒ عصا مائلة قليلاً = وضعيّة لا تشير إلى الحافة المختارة.
- القفز أثناء الانزلاق: المسبار يقبل نقطة على **نفس** الحافة أمامك ⇒ قفزة 20–40 سم بـ`BarJump_Left`.

**التعديلات:**
- `HangJump`: بعد `ParkourInputOverride` ⇒ `Busy = true` ⇒ `UpdateParkourStateData(اتّجاه الهدف)` ⇒ `Busy = false` ⇒ `CalculateClimbJump`.
  (`Busy` يجعل الدالّة تكتب المتغيّرين فقط وتتخطّى التحقّق و`UpdateActiveTarget`.)
- `ABP ▸ EventGraph`: `ReachAngle = atan2( (ProbeTarget − ActiveClimbTarget)·ActorRight , Δz )` بدل العصا.
- دالّة جديدة `ProbeRejectCorner` بعد `ProbeSelect` (في `HangJump` و`ProbeOnInput`):
  ① هدف بوجه مختلف (`ProbeNormal·ActiveClimbNormal < 0.5`) أقرب من 120 سم ⇒ `ProbeResult = 0` (الجدار المتعامد عند الزاوية — للزاوية التلقائيّة).
  ② نفس الوجه (> 0.9) ونفس الارتفاع (±25) وخطّ عند منتصف الشفة (−15 سم، عبر الوجه) **يصيب** ⇒ حافة متّصلة ⇒ `ProbeResult = 4` (لا قفز، لا بديل DPTS، لا Reach).
  الفجوة الحقيقيّة (العوارض) لا يصيبها الخطّ فلا تتأثّر.

**القياس بعد (PIE):**
| الحالة | المونتاج | النهاية | Reach |
|---|---|---|---|
| يسار / يمين صرف | `ClimbJump_Left` / `Right` | y 20129 / 19871، z 332 | −90 / +90 |
| عصا 30° أعلى-يسار / يمين | `ClimbJump_Left` / `Right` (كانت `UpLeft/UpRight`) | z 332 (كانت 357) | −90 / +90 |
| قطري كامل | `UpLeft_Long` / `UpRight_Long` | z 462 في الجهتين | −49 / +42 |
| أعلى | `ClimbJump_Up` | z 436 | 0 |
| قفز أثناء الانزلاق نحو الزاوية الداخليّة (`CORNER_MAIN`) | لا شيء (`ProbeResult 4`) ⇒ يكمل ⇒ `Corner_Inner_L` تلقائيّاً | — | لا ومضة |

**ملاحظات اختبار:** (١) خطوة «الإفلات» في المِسخّر تضغط الجلوس على الأرض ⇒ الشخصيّة **جاثية** (كبسولة 60 بدل 88) ⇒ التعلّق z 300 بدل 326 —
الوضعيّة المرئيّة نفسها (إزاحة الشبكة −62 تعوّض). (٢) بعد انقطاع الحدّ فتح أحدهم مشروع `PLTHVO` على نفس منفذ MCP؛ أُغلق وأُعيد فتح `lastTwoHours`
بمسار مقتبس. المِسخّر محفوظ الآن في ملفّ (`harness.py` في مجلّد المؤقّت) ليُعاد تحميله بعد أيّ إعادة تشغيل.

## ٧٢ · تصحيح: غوص الجدار 3.5 م من مسافة قريبة + الـReach عند الصعود

**بلاغ المستخدم:** «النظر أصبح أسوأ، والعائق 3.5 م من مسافة قريبة يغوص».

**١ · الغوص القريب (قياس عظام اللاعب خلف وجه `Traversable13`، جري 500، الضغط يُعالَج عند **إفلات** الزرّ — المونتاج يبدأ بعد ≈0.06 ث):**
| مسافة بدء المونتاج عن الوجه | أقصى اختراق قبل | بعد |
|---|---|---|
| 206 سم | 33 | 33 |
| 67 سم | **112** (الجذر يمرّ حتى y −799 عند z 88) | — |
| 57 سم | — | **35** |
**السبب:** حتى 0.364 ث (قبل أوّل نافذة warp) يتقدّم جذر الكليب 171 سم بلا تحويل، فلا يتّسع الجدار من أقلّ من ≈170 سم. §٦٣ كان مضبوطاً من ≈194 سم فقط.
**الإصلاح (`AM_L2H_Climb_2_5_run_F_Lfoot`):** نافذة warp جديدة 0.0001 → 0.338 (فجوة 0.026 قبل التي تليها) على الهدف `height`، Forward −70 / Up −273
(نفس هدف النافذة الأولى)، `warp_rotation = false`، `ignore_z_axis = false` (مطابقة للنوافذ الأخرى). الفخّ: النافذة المُضافة تأتي باسم هدف **فارغ** ⇒ لا تعمل؛
يُضبط من `AnimationLibrary.get_animation_notify_events(...)[i].notify_state_class.root_motion_modifier.warp_target_name`.
مدخل الاختبار: ضغط من 250 سم لم يتغيّر (33).
**فخّ قياس مهمّ:** المحرّر في الخلفية لا يتقدّم إلّا مع استدعاءات MCP (≈إطار لكلّ استدعاء) ⇒ توقيت «الضغط عند مسافة كذا» غير موثوق؛ أداة
`set_cpu_throttle_override(force_unthrottle=true)` تحلّ ذلك لكن تُعاد بعد كلّ إعادة تشغيل PIE.

**٢ · الـReach (انحدار من §٧١):** الزاوية من الهدف (`ProbeTarget − ActiveClimbTarget`) خاطئة حين يكون الهدف سطح **صعود** (`ProbeResult = 2`) لأنّ فرق الارتفاع ≈ 0 ⇒ زاوية عشوائيّة.
صارت: `ProbeResult == 1 ? زاوية الهدف : زاوية العصا`. القياس: أعلى ⇒ −1°، يسار ⇒ −90°، ميل 30° يساراً ⇒ −90°.

## ٧٣ · «FREE JUMP» رغم وجود حافة قريبة (لقطة المستخدم: عصا قُطريّة، الشريحة على اليسار)

**التشخيص (قراءة `ProbeScan`/`ProbeSelect`):** المسح متناظر يميناً/يساراً (إزاحة جانبيّة (i−8)×25، i = 0…16). المشكلة في مرشّح `ProbeSelect`: المسافة 50…200 و`dot(offset, intent) > 25`
و **مخروط** `cos ≥ 0.5 + 0.6·sin²θ` (من §٦٢). للعصا القُطريّة 45° ⇒ العتبة 0.8 ⇒ ±37° فقط؛ شريحة أفقيّة على اليسار (45° عن الاتّجاه) أو أعلى بزاوية 70° **تُرفَض** ⇒ لا مرشّح ⇒
`FREE JUMP` ⇒ يسقط. العتبة صارمة أكثر ممّا يلزم (ثمن إصلاح §٦٢)، وتبدو «يسارية» لأنّ الشرائح اليمنى في مشهدك أقرب إلى الاتّجاه القُطريّ.
**الإصلاح:** متغيّر `ProbeRelax`؛ عتبة المخروط = الأصلية − `ProbeRelax`. بعد الحلقة الأولى، إن `ProbeResult = 0` و`ProbeRelax = 0` والعصا قُطريّة (|R|,|U| > 0.1)
⇒ `ProbeRelax = 0.45` (≈±70°) ⇒ استدعاء `ProbeSelect` ثانياً ⇒ `ProbeRelax = 0`. الأقرب-للعصا يبقى الأفضل (الدرجة نفسها)؛ التراخي لا يعمل إلّا حين لا يوجد شيء أصلاً.
**الـReach:** فرق جانبيّ صغير (|dot| < 10 سم) يُصفَّر قبل `atan2` ⇒ النظر للأسفل لا يتأرجح بين حالتَي `ReachLeft/Right` بسبب ضجيج ±180°.
**القياس (منصّة، شريحة أفقيّة فقط على كلّ جهة بلا شرائح قُطريّة، عصا قُطريّة + قفز):** يسار-أعلى ⇒ `ClimbJump_Left` (y 20130)، يمين-أعلى ⇒ `ClimbJump_Right` (y 19869).
يسار/يمين صرف: لم يتغيّر (§٧١).
**لم أستطع إعادة إنتاج موضع لقطة المستخدم نفسه** (شرائح `Bar_Seg_*` المعلّقة وفجواتها 150–500 سم): النزول إلى تعلّق عليها بالمِسخّر لا يعمل (مواضع/اتّجاه). القياس أعلاه على نفس الشرط الهندسيّ.

## ٧٤ · مراجعة تعديلات المستخدم على الحواف + إكمال العارضة (2026-10-03)

**مراجعة التعديلات غير المودَعة** (قورنت بنسخ HEAD داخل `/Game/ZZ_HEAD` — حذفها من المحرّر أسقطه: assert في `WorldPartitionClassDescRegistry`؛ حُذفت من القرص بعد الإغلاق):
| التعديل | الحكم |
|---|---|
| `CornerAutoTry`: `(FailIndex≥0 OR NOT Busy) OR ProbeResult==0/4` | سبب «المشي على الهواء»: بحث انتقال كلّ ثانية وسط الحافة ⇒ أُعيد إلى `FailIndex≥0 AND NOT Busy` |
| مونتاجات الزوايا: نافذة الالتواء الثانية حتى النهاية + Reset متأخّر | لم يغيّر التجاوز، أضاف دوراناً بطيئاً ≈1 ث ⇒ أُعيدت 0.35/0.70 و0.30/0.60 |
| Reach: عيّنة أعلى-يسار، سرعة 25، إخفاء أثناء الحركة لقفزة الحافة فقط | أُبقيت |
| `ProbeOnInput` يعيد الاختيار ما دامت النتيجة 0 · `BeamUpdate` حارس `IsValid` | أُبقيت |

**«الأنيميشن يبتعد عن الحافة الجديدة»:** نافذة الالتواء الأولى (`initial`) في المونتاجات الأربعة `RightOffset = ±40` ⇒ الداخليّة تدخل الجدار 26 سم والخارجيّة تتجاوز الطرف 40 سم.
الآن الداخليّة `±25` (تقف 25 سم عن الجدار) والخارجيّة `∓15` (≈20 سم). مقاس على `CORNER_MAIN/WING` في الاتّجاهين.
**زاوية داخليّة يمنى من `WING`:** تقف بلا `FailIndex` — صندوق `CharacterSpaceValidation` يصيب `LevelBlock13` (شريحة بلا وسم `climb` على وجه `CORNER_MAIN`).
بوسمها `climb` (داخل PIE) تعمل `Corner_Inner_R`. **إصلاح مستوى، لا منطق.**

**العارضة:**
- أيّ زاوية: `BeamUpdate` يأخذ المحور من `GetActorLocalBoundsPCG × Scale` و`Forward/Right` للممثّل. عارضة 45°: يبقى على خطّ المنتصف (x−y ثابت) بعصا مستقيمة أو مائلة 37°.
- `BeamProjectDir(Dir, Record)` يخزّن المركّبة الجانبيّة: `BeamSideVec` و`BeamLean` (−1..1)؛ نداء `IA_Move` وحده يسجّل؛ `BeamInputFrame` يصفّرهما بلا مدخل.
- قفز: على طول العارضة كان يعمل (150 سم فجوة). جانبيّ: `OnJumped ▸ LaunchCharacter(BeamSideVec × 130)` ⇒ يهبط على عارضة موازية على بعد 150 سم.
- ميلان: `balance_l/r` (ATS مضافة، أُعيد استهدافها) فوق `L2H_BeamWalk_BS` بـ`ApplyAdditive`، ألفا من `BeamLean` (FInterpTo 6).
- 11 كليب ATS أُعيد استهدافها (`RTG_ALS_to_UEFN`)؛ أُزيلت إشعارات `BeamWalk_Footsteps` (BPs لا تُترجَم ⇒ PIE يتوقّف على نافذة؛ استخدم `start_pie(on_compile_errors=suppress)`).
- ساحة التجربة `BeamTest`: أُضيفت `Diag45` + `PlatD` + `GapA/GapB` + `Side` (المستوى غير محفوظ من طرفي).

## ٧٥ · إغلاق الحواف والعوارض (2026-10-03، بعد الظهر)

**السبب الجذري لأغلب بلاغات اليوم:** المستخدم يتحرّك بالأسهم، و`IMC_Sandbox` (من GASP) كان يربطها بـ`IA_Move_WorldSpace`
(معالجه `AddMovementInput` فقط) ⇒ DPTS لا يستلم `UpdateParkourStateData` ⇒ `Right Input = 0` دائماً: لا أنيميشن تنقّل، لا توقّف عند
نهاية الحافة (يسير في الفراغ)، لا صعود بالعصا. كلّ اختباراتي تحقن `IA_Move` فنجحت. اكتُشف بمسجّل صامت داخل المحرّر
(slate post-tick) أثناء لعب المستخدم. **الإصلاح:** الأسهم ⇒ `IA_Move` بنفس مُعدِّلات WASD (`6e3d9a06`).

**بقيّة إصلاحات اليوم (كلّها مقاسة في PIE، Listen Server):**
| الإيداع | ماذا |
|---|---|
| `3ff73187` | `CornerAutoTry` أُعيد لشرطه؛ إزاحات نوافذ الزوايا ±40 ⇒ داخليّة 25 / خارجيّة 15 |
| `732bcabe` `6b37ffe2` `1aa7f373` | عوارض بأيّ زاوية (`GetActorLocalBoundsPCG`)، كشف بـ`GetMovementBaseActor`، ميلان `balance_l/r`، قفز من العارضة: XY = 650 باتّجاه الحركة + 300 جانبي (+100 Z) ⇒ ~575 سم |
| `04423083` `3e169525` | `BP_L2H_Beam`: وسوم `beam/climb/ledge` افتراضيّاً + `HangWall` (صندوق تحت العارضة يحجب Visibility فقط) ⇒ التعلّق الجانبي = حافة (`FullContact`، تنقّل 90 سم/ث بدل 0) |
| `1aa7f373` `0ad43a02` | `HangUpAuto`: الأعلى 0.35 ث ⇒ صعود؛ القفز وحده ⇒ صعود؛ `ClimbDownTry` يصل لوجه العارضة (قرفصاء واحدة ⇒ تعلّق) |
| `4cc2353b` | `BeamUpdate` لا يطفئ قيد المستوى أثناء تسلّق DPTS |

**ملاحظات:** `LevelBlock13` (شريحة بلا `climb` على وجه `CORNER_MAIN`) تمنع الزاوية الداخليّة اليمنى — إصلاح مستوى.
ساحة `BeamTest` + سلسلة `Run/Gap300/Gap400` شرق `PlatB` في المستوى (حفظه بيد المستخدم).

## ٧٦ · الدحرجة (Elden Ring) والتزحلق (GASP) (2026-10-03، مساءً)

**الدحرجة** — زرّ واحد مثل Elden Ring: `IA_Dodge` (Tap ≤ 0.22 ث) على Shift وLB، نفس زرّي `IA_Sprint` (الضغط المطوّل يبقى ركضاً).
| الشيء | القيمة المقاسة |
|---|---|
| `AM_L2H_Roll` (من `A_Roll_IdleFwd` محوَّل Manny⇒UEFN بـ`RTG_Manny_to_UEFN`) | 446 سم في 1.0 ث، يدور فوراً نحو التسارع (اتّجاه العصا بالنسبة للكاميرا) |
| `AM_L2H_Backstep` (GASP `Jump_B_Start` 0–0.66 + `Jump_B_Land_Stand_Light` 0.62–1.8، ×1.2) | ~87 سم للخلف، بلا عصا |
| `ANS_L2H_IFrames` | دحرجة 0.08–0.45 ث، خطوة 0.03–0.33 ث؛ وسم `State.Invulnerable.Dodge` (Loose) على `L2H_AbilitySystemComponent` المضاف للشخصيّة |
| تخزين الإدخال | نقرة ≤ 0.35 ث قبل نافذة الإلغاء (دحرجة 0.70، خطوة 0.95) ⇒ مؤقّت يعيد `DodgeTry` ⇒ دحرجة متّصلة عند 0.70 بالضبط |
| الممنوعات | في الهواء، `Busy`، عبور GASP، على العارضة، أيّ مونتاج آخر |
| الشبكة | `Server_Roll/Backstep` (الخادم يحسب الاتّجاه من تسارع العميل) ⇒ `Multicast_*` للبقيّة؛ العميل يتبعه الخادم بإطار، بلا تصحيح |

**تنبيه قياس:** `call_method('DodgeTry')` من Python على عميل لا يُرسل RPC — الاختبار عبر حقن `IA_Dodge` في LocalPlayer العميل.
**الجذر المسطَّح مجدّداً:** المحوّل الجديد وضع كلّ الإزاحة في الحوض؛ نُقلت الإزاحة الأفقيّة إلى `root` بمفاتيح المسار (الوضع العالمي مطابق).

**C++:** `AL2H_TimeDirector::ApplyTimeDamage` يتجاهل أيّ هدف عليه `State.Invulnerable.*` (عبر `GetAbilitySystemComponentFromActor`).
Live Coding فشل بـ`LNK2011 precompiled object not linked in` على كلّ الوحدات (Monolith أيضاً) — مشكلة بيئة؛ يُبنى عند فتح المحرّر التالي.

**التزحلق** — `SlideUpdate` في آخر `UpdateMovement_PreCMC` + `AM_L2H_Slide` (Default 0.5 ث ⇒ Loop يتكرّر ⇒ `OutRun`/`OutStand`/`OutCrouch`)،
مقاطع GASP `Slide_FootOut` منسوخة بلا حركة جذر + `ForceRootLock` (الفيزياء تحرّك الجسم). مُختارات المشروع لا تحوي قواعد `PSD_*_Slide`.
| القاعدة | القيمة |
|---|---|
| بدء يدويّ | قرفصاء والسرعة الأفقيّة ≥ 400 |
| بدء تلقائيّ | أرض بميل ≥ 28° (N.z ≤ 0.883) وليس صعوداً (v·N_xy ≥ −30) |
| أثناءه | احتكاك 0، `BrakingDecelerationWalking` 0، كبح يدويّ 400 (مستوٍ) / 100 (منحدر)، `MaxWalkSpeed` = السرعة الحاليّة (العصا توجّه فقط، تسارع 150)، + الجاذبيّة على مستوى الأرض، سقف 1600 |
| خروج | ترك الأرض ⇒ إيقاف المونتاج؛ < 150 على ميل < 15° ⇒ قرفصاء `OutCrouch`، عصا `OutRun`، وإلّا `OutStand` |
| مقاس | ركض 700 + قرفصاء ⇒ 1.5 ث / 6.4 م بلا عصا؛ منحدر 30° من السكون ⇒ 0→867 ثم يكمل على المستوي ويخرج؛ عميل Listen: الخادم يطابق بإطار |

**الخطوة التالية (خارطة):** الجري على الجدار (DPTS Wallrun) ⇒ تأرجح العمود (UTA BarSwing) ⇒ الحبل/الخطّاف ⇒ Zipline ⇒ العمود العموديّ ⇒ السلالم.

## ٧٧ · ملاحظات المستخدم على الدحرجة والتزحلق والعوارض (2026-10-03، ليلاً)

| البلاغ | السبب | الإصلاح (مقاس في PIE) |
|---|---|---|
| الخطوة للخلف «يختفي ويطير للأمام ثم يعود» | المونتاج كان مركّباً من مقطعي قفز GASP | `AM_L2H_Backstep` = `RM_Dodge_back_02` من حزمة 114 (محوَّل + نقل الجذر): 170 سم في 0.8 ث، الجسم على الكبسولة (إزاحة 0) |
| حزمة «114 Roll and Dodge» لا تُحمَّل | المسار فيه مسافات ⇒ `Invalid LongPackageName` | نُسخت إلى `Content/DodgeAndRoll` (اسمها الأصلي، كلّ مراجعها الداخليّة `/Game/DodgeAndRoll/`)؛ القديم محجوز من المحرّر ويُحذف بعد إغلاقه |
| التزحلق يعمل وهو يمشي | عتبة 400 تشمل سرعة الجري العاديّ (500) | يدويّ: سرعة ≥ 600 (Sprint)؛ منحدر: ≥ 28° مع Sprint، أو ≥ 38° بأيّ سرعة |
| القفز على العارضة مبالغ | 650 للأمام + 300 جانبيّ | `BeamJumpLaunch`: عصا جانبيّة ⇒ يدور نحو الجانب، `BeamFindSide` يجد العارضة المجاورة ≤ 4 م ويحسب السرعة = المسافة ÷ زمن الطيران؛ Air Control = 0 أثناء القفزة ويعود عند الهبوط. وإلّا قفزة عموديّة في المكان |

مقاس: عارضتان بينهما 250 سم ⇒ سرعة 320، هبوط على مركز الثانية. أمام + قفز ⇒ يرتفع ويعود لنفس العارضة.
**تنبيه اختبار:** القرفصاء Toggle وتبقى بين التجارب؛ الشخصيّة المقرفصة لا تقفز (أفسد ثلاث تجارب).
**تنبيه أداة:** `write_graph_dsl` على `BeamFindSide` أبقى العقد القديمة مرّة (تتبّع منفصل + Return قديمان) — أُزيلت يدويّاً؛ اقرأ ملخّص الدالّة بعد كلّ إعادة كتابة.
**ملفّ GASP:** `PSD_SM_CMC_Transitions` معدَّل على القرص ويشير لنسختيّ `Jump_B` — لا يُودَع؛ يُستعاد بـgit بعد إغلاق المحرّر.

## ٧٨ · الدحرجة على زرّ مستقلّ + دحرجة الغطس (2026-10-03، ليلاً)

- `IA_Dodge`: Pressed (فوريّ) على **Alt** و**B**؛ القرفصاء على اليد انتقلت من B إلى **L3** (الكيبورد C كما هو). الركض السريع يبقى Shift/LB.
- `AM_L2H_RollDive` من `RM_Roll_dive_front` (حزمة DodgeAndRoll، محوَّل + نقل الجذر): 496 سم، حصانة 0.1–0.6 ث، إلغاء 1.2.
- `DodgeStart`: مع اتّجاه + سرعة ≥ 600 ⇒ غطس (Type 2، `Server_Dive`/`Multicast_Dive`)؛ مع اتّجاه أبطأ ⇒ دحرجة؛ بلا اتّجاه ⇒ خطوة للخلف.
- مقاس في PIE: ركض سريع ⇒ غطس 480 سم؛ جري 500 ⇒ دحرجة؛ وقوف ⇒ خطوة 170 سم؛ الجسم على الكبسولة في الثلاثة.
- C++ بُني بـUBT والمحرّر مغلق (نجح) ⇒ تجاهل الضرر مع `State.Invulnerable` صار مترجَماً.
- **انهيار:** أوّل حفظ لـ`IMC_Sandbox` بعد إعادة التشغيل ⇒ `appError` (MoveFile، الملفّ محجوز، Error 32). بعد إغلاق النسخة المنهارة وإعادة الفتح حُفظ عاديّاً. `write_graph_dsl` أبقى العقد القديمة في `DodgeDo`/`DodgeServer` مرّة أخرى — حُذفت وفُحص الملخّص.

## ٧٩ · الجري الجانبيّ على الجدار (It Takes Two) (2026-10-03، ليلاً)

- **الجدار:** أيّ ممثّل عليه الوسم **`siderun`** (لا `wallrun`: هذا الوسم يُشغّل حالة `WALLRUN` المدمجة في DPTS — طيران بسرعة ثابتة 850 بلا أنيميشن ويستمرّ بعد نهاية الجدار).
- **البدء** (`SideRunCanStart`، على المتحكّم المحلّيّ): في الهواء، سرعة أفقيّة ≥ 350، العصا مضغوطة، لا DPTS/Traversal/عارضة، جدار موسوم بتتبّع جانبيّ 80 سم، الاقتراب ليس مواجهاً (|cos| < 0.8)، وليس آخر جدار قبل لمس الأرض.
- **الحركة** (`SideRunPhys`): جاذبيّة 0 وتحكّم هوائيّ 0؛ السرعة على امتداد الجدار 550–800، التصاق 80 نحو الجدار، الارتفاع `200 − 400·t` (قوس يصعد ~45 سم ثم ينزل)؛ الدوران باتّجاه الجري.
- **الانتهاء:** لمس الأرض، أو انتهاء الجدار، أو 1.4 ث، أو ترك العصا.
- **القفز من الجدار:** القفز يمرّ عبر `Jump` العاديّ (`JumpMaxCount` يُرفع أثناء الجري) ⇒ `OnJumped` ⇒ `SideRunJumpLaunch`: 500 بعيداً عن الجدار + 70% من السرعة + 600 للأعلى. يُتيح السلسلة بين جدارين متقابلين.
- **الأنيميشن:** `AM_L2H_WallRun_L/R` (Start → Loop متكرّر → End) من `FreeAnimationLibrary/retarget/…WallRun_*` (على هيكلنا، بلا حركة جذر). Left = الجدار على اليسار (القدمان عند +X في فضاء الشبكة).
- **الشبكة:** `Server_SideRunStart/End` و`Multicast_SideRunStart/End`؛ الخادم يعيد كشف الجدار بنفسه بمدى 160 سم (بـ80 فشل على الجدار الثاني لأنّ الاقتراب المائل يقصّر البعد العموديّ). القفز يُحسب في `OnJumped` على الطرفين.
- **مقاس في PIE:** جدار أيمن 1.3 ث / 7.8 م؛ قفز إلى الجدار المقابل والتقاطه؛ مواجهة الجدار ⇒ لا جري؛ جدار بلا وسم ⇒ لا جري. Listen Server: الخادم يتبع بإطار واحد على الجدارين، أقصى فرق 26 سم (متوسّط 14)، بلا تصحيحات.
- **ملاحظة اختبار:** المحرّر المصغَّر يعمل بـ3 إطارات/ث فتضيع نقرات الحقن؛ يجب استعادة النافذة. `PlayAnimMontage` بمعامل موضعيّ ذهب إلى `InPlayRate` — استعمل `:AnimMontage`.

## ٨٠ · قدم الدحرجة، الدحرجة المقطوعة، ومضمار الجري على الجدار (2026-10-03، ليلاً)

- **القدم الثابتة:** عقدة Foot Placement في `ABP_L2H_Base` لم تكن تقرأ أيّ منحنى تعطيل (`DisablePelvisCurveName` و`DisableLegCurveName` فارغة). مع طيّ الساقين في الدحرجة كانت تدفع الحوض حتى 78 سم وتُبقي القدمين. الإصلاح: ربط `disablepelvisadjustment` و`disablerightfootik`/`disableleftfootik` في العقدة، وإضافة المنحنيات الثلاثة بقيمة 1 على مقاطع الدحرجة/الخطوة/الغطس. منحنيات GASP في مقاطع Traversal صارت فعّالة أيضاً.
- **عظام IK:** في `L2H_RM_Dodge_back_02` و`L2H_RM_Roll_dive_front` و`L2H_A_Roll_IdleFwd` كانت `ik_foot_l/r` بعيدة عن القدم الحقيقيّة حتى 111 سم (من التحويل). أُعيد خَبزها من عظام FK إطاراً بإطار (فرق 0).
- **مقاس:** فرق كلّ عظام الساق عن الأنيميشن: الدحرجة ≤ 4 سم، الخطوة ≤ 1.4، الغطس ≤ 2.1 (كان حتى 78).
- **الدحرجة المقطوعة:** `AM_L2H_Roll` بُدّل خارج الجلسة (21:46) إلى `L2H_RM_Roll_front` (1.97 ث، 350 سم) لكنّ طول المونتاج بقي 1.0 ⇒ يخرج عند 0.72. أُعيد حساب الطول (حفظ + إعادة تحميل). الحصانة نُقلت إلى 0.3–1.0 والإلغاء إلى 1.5 لتناسب المقطع الجديد. مقاس: من المشي يكتمل حتى 1.7 ثم يعود للمشي؛ السلسلة عند 1.5.
- **الجري الجانبيّ:**
  - ذاكرة «آخر جدار» صارت بالمكوّن (`SideRunLastComp`)، فجدران Blueprint واحد تتسلسل.
  - اتّجاه الجري يُؤخذ من وجه الشخصيّة، فالاصطدام لم يعد يعكسه.
  - المنحنى صار أسطح `150 − 220·t` (يصعد ~50 سم ويعود تقريباً).
  - الانسداد (سرعة < 250) يُنهي الجري.
- **المضمار** `BP_L2H_SideRunCourse` (وسم `siderun`، هضبة 1.5 م بمنحدر): ممرّ جدار واحد + جدار ثانٍ بعد فجوة، رواق جدارين متقابلين (4.2 م)، وحفرة 5 م بجدار بجانبها. وُضعت نسخة في `DefaultLevel` عند (−3000, 0, 150) باتّجاه 180 — المستوى غير محفوظ.
- مقاس في المضمار: الجدار الأوّل ✔، عبور الحفرة على الجدار ✔، الرواق يمين ⇒ يسار ⇒ يمين ✔.
- **ملاحظة:** تعديل مكوّنات Blueprint عبر الأداة لا يحدّث النسخ الموضوعة في المستوى — احذفها وضعها من جديد.

## ٨١ · الغطس لكلّ دحرجة باتّجاه + دحرجة للخلف (2026-10-04)

- طلب المستخدم بعد المشاهدة: دحرجة الركض (الغطس) أسرع وأكثر تفاعليّة من دحرجة المشي البطيئة (1.97 ث).
- `DodgeStart`: عتبة السرعة للغطس صارت −1 (كانت 600) ⇒ أيّ دحرجة باتّجاه = `AM_L2H_RollDive`. `AM_L2H_Roll` لم يعد مستعملاً (باقٍ). مقاس: مشي 250 ⇒ غطس يكتمل.
- الخطوة للخلف ⇒ **دحرجة للخلف**: `DynamicFalling/RM_Roll_back` (1.17 ث) حُوِّل بـ`RTG_Manny_to_UEFN` ⇒ `L2H_RM_Roll_back`، نُقل الجذر (340 سم)، خُبزت عظام IK، وأضيفت منحنيات Foot Placement. `AM_L2H_Backstep` يشير إليه الآن؛ الحصانة 0.05–0.5.
- مقاس: 307 سم للخلف، الشبكة على الكبسولة (0)، الساقان ≤ 3.5 سم عن الأنيميشن.
- `DynamicFalling` فيه 10 دحرجات (8 اتّجاهات + Run_F/B) بطول ~1.17 ث — صالحة لدحرجة هبوط أو دحرجات اتّجاهيّة لاحقاً.

## ٨٢ · القفز على العارضة للأمام وللخلف بمدى (2026-10-04)

- كانت العصا على طول العارضة + قفز ⇒ قفزة عموديّة في المكان.
- `BeamFindAhead(Dir)` الجديدة: تتبّعات للأسفل كلّ 20 سم من 60 إلى 500 سم في اتّجاه القفز. تبحث عن نهاية العارضة الحاليّة (فجوة)، ثم أوّل سطح بعدها بارتفاع قريب. تُرجع المسافة + 35 سم.
- `BeamJumpLaunch`: العصا الجانبيّة كما هي. على طول العارضة (من التسارع، فيصل الخادم أيضاً): يستدير للاتّجاه، والسرعة = المسافة ÷ زمن الطيران (120–650)، أو 280 إن لم يجد هدفاً. Air Control = 0 أثناء الطيران.
- مقاس (ساحة `BEAM_TEST`):
  - أمام فوق فجوة 3 م ⇒ هبط على `Gap300` (470 سم).
  - خلف ⇒ استدار وهبط على `Run`.
  - أمام فوق فجوة 4 م ⇒ هبط على `Gap400` بعد الحافّة بـ85 سم.
- **فخّ DSL:** `(* Dir _d)` يُرقّي `_d` نفسه إلى متّجه إن استُعمل في مكان آخر ⇒ استعمل المكوّنات `(.x)` `(.y)`. والـ`return` بقيمة محسوبة داخل `for` يُرفض ⇒ متغيّر عضو ثم `return` بعد الحلقة.

## ٨٣ · دحرجة الهبوط من ارتفاع (2026-10-04)

- `DynamicFalling/RM_Roll_front` (1.17 ث، 320 سم، يبدأ منخفضاً) ⇒ `L2H_RM_Roll_front_Land`: تحويل + نقل الجذر + خَبز IK + منحنيات Foot Placement. والمونتاج `AM_L2H_LandRoll` (دخول 0.05، خروج 0.3).
- `LandRollTry` بين `Event OnLanded` و`Custom OnLanded Event`. الشروط: سرعة الهبوط < −850 (سقوط ≥ ~3 م)، والعصا مضغوطة، ولا DPTS/Traversal/تزحلق/مونتاج. يدور نحو العصا ويشغّل المونتاج. يعمل على المالك والخادم، والخادم يكرّر مونتاج حركة الجذر للبقيّة.
- مقاس: 5 م + عصا ⇒ دحرجة ثم جري 500 مباشرة؛ 5 م بلا عصا ⇒ لا؛ 2 م + عصا ⇒ لا.
- **تنبيه أداة:** `EditorAssetLibrary.save_asset` يُرجع False للأصول المُنشأة حديثاً ولا يكتب شيئاً؛ `EditorLoadingAndSavingUtils.save_packages` يكتبها.

## ٨٤ · بنود مؤجّلة: الإمساك بالعارضة سقوطاً، leap_long، LevelBlock13 (2026-10-04)

- **الإمساك بالعارضة أثناء السقوط:** يعمل أصلاً عبر بحث DPTS في الهواء (العارضة موسومة `climb/ledge`). مقاس: سقوط من 2.5 م على بعد 5 و30 سم من وجه العارضة ⇒ `LEDGE_CLIMB` (تعلّق) خلال 0.7 ث. لا تعديل.
- **`leap_long`:** موجود ومربوط منذ §٣٢ (192 سم، مونتاجات `_Long` لثمانية اتّجاهات، صفوف 145–250 في `DPTS_ClimbJump_SelectionTable ▸ L2H`). لا تعديل.
- **`LevelBlock13`:** أُضيف وسم `climb` في المحرّر (المستوى غير محفوظ — يحفظه المستخدم) ⇒ الزاوية الداخليّة اليمنى من `WING`.
- **فخّ اختبار:** نقل الشخصيّة من فوق عارضة بـPython يُبقي قيد المستوى ⇒ يُسحب إلى خطّ العارضة. انقل أوّلاً إلى أرض عاديّة وانتظر هبوطه.

## ٨٥ · التأرجح على العمود الأفقيّ (Bar Swing) (2026-10-04)

**الأصول:** `BP_L2H_SwingBar` (عمود 4 م على محور Y المحلّيّ، بلا تصادم، عمودان، وسم `swingbar`)؛ `L2H_BarHangIdle` (نسخة من `FreeAnimationLibrary/.../BarSwing_ForwardAcross_HangingIdle` + منحنيات Foot Placement)؛ `AM_L2H_BarHang` (حلقة).

**المنطق (`BarUpdate` في `UpdateMovement_PreCMC`):**
- **الإمساك تلقائيّ في الهواء** إذا كانت اليدان (مركز الكبسولة + 104) ضمن 85 سم من خطّ العمود (`BarFind`: أقرب نقطة على العمود ±180 سم). اتّجاه التأرجح = عموديّ على العمود بإشارة السرعة.
- **بندول حقيقيّ** (`BarPhys`، على المالك والخادم): R = 106، g = 1176 ⇒ `θ'' = −11.09·sinθ + دفع − 0.3·ω`.
  - الدفع = (العصا على اتّجاه التأرجح)² × 5 باتّجاه الحركة، حتى 80°؛ والزاوية ≤ 100°.
  - الحركة بالسرعة نحو نقطة الدائرة في وضع Flying (`MaxFlySpeed` 2500 أثناءه، 600 بعده)، والسرعة ≤ 1200.
- **الجسم** (`BarMeshPose`، على كلّ الأجهزة من موضع الكبسولة نسبةً للعمود): `Mesh` يدور حول مركز الكبسولة بالزاوية ⇒ اليدان على العمود.
- **القفز يُفلت** (`SideRunJumpGate` ⇒ `BarRelease`): سرعة البندول المماسّيّة + 200 للأمام + 450 للأعلى.
- **الشبكة:** `Server_BarStart/Release` و`Multicast_BarStart/End`، ومهلة 0.5 ث قبل إمساك جديد.

**السبب الجذريّ لـ«اليدين خارج العمود» (حتى 190 سم):** عقدة `Offset Root Bone` في ABP تعاكس دوران وانتقال المكوّن أثناء المونتاج (وضع Release) ثمّ تحرّرها بنصف عمر 0.2 ث.
- **الإصلاح:** `Save Cached Pose (L2H_PreRootOffset)` بعد `Slot_0`، و`Blend Poses by bool (IsBarSwinging)` يتخطّى العقدة أثناء التأرجح فقط.
- **محاولات لم تكفِ:** نصف عمر 0، أوضاع Interpolate، `bClampToTranslationVelocity`، و`MaxRotationError` (الدبّوس مخفيّ فالربط لا يُطبَّق).
- **أُبقي:** `L2H_AOOn` و`L2H_LeanOn` متغيّرات ABP (تساوي الأصل خارج التأرجح)، و`Get_OffsetRoot*` تُرجع Interpolate/0 أثناءه.

**مقاس:**
- اليد–العمود 18–21 سم عند كلّ الزوايا (= تباعد اليدين)، كان حتى 190.
- إفلات عند 39° ⇒ 676 للأمام و817 للأعلى، قمّة z 648، هبوط على الطرف البعيد من حفرة المضمار (−5152).
- Listen Server: الخادم يتبع بإطار، وفرق الموضع 15 سم متوسّطاً، والهبوط واحد.

**فخاخ:**
- DSL يكتب اسم Slot في `IsSlotActive` كـ`None` — أُعيد `DefaultSlot` يدويّاً.
- دوالّ ABP ذاتيّة (`IsMoving`/`EnableAO`) تُحلّ لصنف GASP الأصليّ في DSL — أضفها بـ`add_node CallFunction`.
- عُدِّلت ثلاثة أصول أثناء PIE فلم تُحفظ (`save_asset` = False) — أوقف PIE دائماً قبل الحفظ.

## ٨٦ · السلالم والعمود العموديّ (2026-10-04)

**الأصول (`Content/_Project/Movement/Climb/`):**
- `BP_L2H_Ladder`: وسم `ladder`، ارتفاع 440، درجات كلّ 29 سم بدءاً من 12، بلا تصادم. واجهة الصعود هي X+ للممثّل.
- `BP_L2H_Pole`: وسم `pole`، أسطوانة نصف قطرها 8 وطولها 600، بلا تصادم.
- المقاطع (نسخ من `FreeAnimationLibrary/retarget` مع إعادة خبز أقدام IK ومنحنيات Foot Placement):
  - `L2H_LadderUpLoop`، مقطع وضعيّات بلا حركة جذر، يصعد 58 سم في الدورة.
  - `L2H_PoleUp`، يصعد 125 سم في الدورة.
  - `L2H_LadderTopOut` بحركة جذر (+150 للأعلى، +65 للأمام).
- المونتاجات: `AM_L2H_LadderClimb` و`AM_L2H_PoleClimb` (حلقة)، و`AM_L2H_LadderTopOut`.

**المنطق (`L2H|Ladder` في `BP_L2H_Base`، و`LadUpdate` بعد `BarUpdate` في `UpdateMovement_PreCMC`):**
- **الإمساك (`LadFind` و`LadCanStart`):**
  - السلّم: العمق بين −68 و+15 من مستوى السلّم، والإزاحة الجانبيّة ±30.
  - العمود: المسافة من المحور أقلّ من 70.
  - على الأرض يلزم دفع العصا نحو البنية (ضرب نقطيّ > 0.6)، وفي الهواء يكون الإمساك تلقائيّاً.
- **الحركة (`LadPhys`):**
  - وضع Flying، والسرعة = (الهدف − الموقع)/dt.
  - العصا للأمام أو للخلف (نسبةً إلى الكاميرا) تحرّك `LadZ` بسرعة 140 للسلّم و150 للعمود.
  - عند إفلات العصا على السلّم ينتقل اللاعب إلى أقرب درجة (`SnapToGrid` بخطوة 58).
  - العصا جانباً على العمود تدير اللاعب حوله بسرعة 110°/ث.
- **الأنيميشن (`LadAnim`):** موضع المونتاج = Fraction(الارتفاع/الدورة)، فاليدان تتبعان الدرجات صعوداً ونزولاً، وتُعاد حتى عند اللاعبين البعيدين.
- **الخروج:**
  - أعلى السلّم: `TopOut` بحركة جذر.
  - الأسفل: إفلات.
  - القفز: للخلف، أو باتجاه العصا (يُتجاهل الاتجاه إن كان نحو السلّم بضرب نقطيّ ≥ 0.3، أو نحو العمود ≥ 0.85).
  - سرعة القفز من العمود 560 أفقيّاً و430 عموديّاً، وتصل إلى عمود على بعد 3.5 م.
- **ABP:** `IsBarSwinging` صار يساوي `IsBarSwinging OR IsLadClimbing`، فيُتجاوز `Offset Root Bone` ويُطفأ AO والميل أثناء التسلّق.
- **الشبكة:** `Server_LadStart/Jump/Drop/TopOut` و`Multicast_LadStart/End/TopOut`، بنفس نمط العارضة.

**القياس:**
- فجوة اليد عن السلّم أو العمود: الوسيط 6–7 سم، والأقصى 11 سم.
  - استثناء: 0.1 ث عند لحظة الإمساك، حتى 84 سم، ثمّ ينتقل الجسد إلى مكانه.
- الصعود مع الخروج إلى المنصّة، والنزول مع الإفلات، والقفز للخلف (3.6 م): كلّها تعمل.
- القفز من عمود إلى عمود (3.5 م) يعمل، وكذلك الدوران حول العمود.
- Listen Server:
  - العميل مقابل نسخته على الخادم: متوسّط الفرق 1.7 سم على السلّم و3.3 سم على العمود.
  - الخادم كما يراه العميل: 1.5 سم.

**فخاخ:**
- `UpdateRotation_PreCMC` في GASP يضبط `OrientRotationToMovement` بمعدّل دوران −1 (فوريّ)، فيدير الممثّل نحو العصا، وتبتعد اليدان 50 سم. الحلّ: `LadPhys` يطفئ الخيارين كلّ إطار، لأنّه يُستدعى بعدها.
- كاتب DSL: `else` يبدأ بـ`if` يُدمج إلى `elif`، فتُبتلع العبارات التي تليه داخل ذلك الفرع. لا تبدأ `else` بـ`if`.
- موضع المونتاج عند نهاية المقطع تماماً يُنهي المونتاج، لذلك يُحصر الموضع داخل المقطع، ويُعاد التشغيل إن توقّف.
- أداة `unreal_editor` أعادت 401: بدأتُ PIE بـ`LevelEditorSubsystem`، وضبطتُ وضع Listen بـ`set_property_at_path` على `Default__LevelEditorPlaySettings`.

## ٨٧ · استلام عمل Aura، وزرّ التفاعل، والسلّم بالضغط (2026-10-04)

**الأساس (`c4d617cb`):** حفظتُ عمل Aura (التفاعل، والحقيبة، والأسلحة، والضرر، والحفظ، و`BP_L2H_TestPawn`) كما هو، بعد build ناجح واختبار سريع.
- **الأسلحة تعمل:** الإطلاق ينقص الذخيرة من 8 إلى 5، وإعادة التلقيم تملؤها، والتبديل ينقل من المسدّس إلى البندقيّة (30 طلقة).
- **مكوّن التفاعل لم يكن يعمل إطلاقاً:** كان ينقصه `bAutoActivate`، فلم يكن يُفعَّل ولا يُحدَّث كلّ إطار. أُصلح في `3ac0ebf8`.
- **`GM_L2H_TestPawn` لا يُظهر بيدق الاختبار:** `GM_L2H_Time` يختار يوسف أو سامر ويتجاهل الإعداد. للاختبار وضعتُ البيدق في المستوى مع `AutoPossess Player0`.

**زرّ التفاعل (المثلّث / E) على شخصيّة الحركة (`3ac0ebf8`):**
- أُضيف `UL2H_InteractionComponent` إلى `BP_L2H_Base`.
- `IA_Interact` كان مربوطاً بالزرّين في `IMC_Sandbox` من قبل، لكن بلا من يستقبله. الآن يستدعي `InteractPressed`، وهذا يفعل أحد ثلاثة أشياء:
  - **سلّم من الأسفل:** الضغط يُخزَّن 0.3 ث، أو 1 ث إن كنت في الهواء، و`LadCanStart` يشترطه للسلّم فقط.
  - **سلّم من الأعلى:** `LadFindTop` يبحث في المنصّة على عمق بين 5 و130 من مستوى السلّم، ثم يُنفَّذ `LadTopApply`:
    - تثبيت الموضع على عمق +60 من مستوى السلّم، مع الالتفاف ليواجه الحافّة.
    - تشغيل `AM_L2H_LadderTopIn`، وهو `Ladder_Climb_Down_Start1` مع حركة الجذر: يدور 180° ويتقدّم 88 وينزل 178.
    - التصادم مُطفأ لمدّة 2.1 ث، ثم يبدأ التسلّق.
  - **غير ذلك:** `RequestInteract`.
- **العمود:** يبقى إمساكه تلقائيّاً.
- **البرومبت:** `WBP_L2H_InteractPrompt` يعرض «Climb» أو «Climb down» أو `<الفعل> <الاسم>` القادم من المكوّن.

**القياس:**
- بلا ضغط لا يحدث إمساك؛ وقف اللاعب عند الجدار 1.4 ث.
- الضغط يُمسك السلّم، ثم الصعود والخروج إلى المنصّة.
- الدخول من الأعلى ينتهي على بُعد 4 سم من موضع التسلّق، واليدان على بُعد 1–7 سم.
- الضغط في الهواء يُمسك السلّم على ارتفاع 158 سم.
- الباب يفتحه الخادم والعميل، ويظهر مفتوحاً على الطرفين (95°).
- العميل يدخل من الأعلى بفرق 3.7 سم في المتوسّط، ويُمسك من الأسفل بفرق 3.7 سم.

**إصلاح:** `SideRunFindWall` كان يستدعي `ActorHasTag` على HitActor فارغ، لأنّ AND في BP لا يتوقّف عند أوّل false. صار الوسم يُقرأ من `select(hit, HitActor, GameState)`.

**الخطّاف كسلاح (`bb432f4e`، C++):**
- `EL2H_WeaponType::Hook`.
- `FireAbilityOverride` في بيانات السلاح: إن كان مضبوطاً، يمنح `WeaponFire` تلك القدرة عند أوّل استعمال ويفعّلها بدل الإطلاق.

**فخاخ:**
- عند Listen Server بلاعبين يوجد لاعبان محلّيّان، فـ«أحدث subsystem» يعود للاعب 1. اللاعب 0 هو `subs[-N]`.
- المحرّر في الخلفيّة يعمل بـ3 إطارات/ث. الحلّ: `bThrottleCPUWhenNotForeground=false` عبر `set_property_at_path`، وأعيده بعد الانتهاء.
- `BP_L2H_Door` أصله في المنتصف: ضعه على ارتفاع z=100، وإلّا يقع نصفه تحت الأرض ويمرّ خطّ النظر فوقه.

**مؤجّل:**
- **إعادة أب `BP_L2H_Base` إلى `AL2H_BaseCharacter`:** رفضها نظام الأذونات، وتنتظر قرار المستخدم.
- **العمود في الهواء على الشبكة:** الخادم يحسب زاوية الإمساك من موضعه هو، فيبقى بعيداً 14 سم عن العميل.

## ٨٨ · ملاحظات المستخدم: السلّم، والعمود، والدحرجة، والعارضة، والقفز للخلف، والوقوف فوق العمود (2026-10-04)

**السلّم (`9f33c3e3`):**
- **السرعة:** 100 سم/ث، ودورة اليد كلّ 58 سم.
- **مسافة اليدين:** 0–3 سم من مستوى السلّم (D=32).
- **الخروج إلى المنصّة:**
  - التصادم مُطفأ أثناء المقطع.
  - `L2H_LadderTopOut` صار بقفل جذر `REF_POSE`، لأنّ جذره في الإطار الأوّل على z=−150.
  - النتيجة: صعود ناعم من 375 إلى 524 بلا توقّف.
- **القفز عن السلّم:** 220 أفقيّاً و330 للأعلى، فيهبط على بُعد 1.8 م.
- **الدحرجة:** `SideRunJumpGate` يمنع القفز ما دام `LandRoll` أو `Roll` أو `RollDive` أو `Backstep` يعمل.

**العمود العموديّ (`a1988e60`):**
- **التسلّق:** مقطع ATS `move_upward`، بقفل الجذر وطور مربوط بالارتفاع: `Fraction(h/62)·0.88`. اختفى انزلاق اليد، والمسافة 3–5 سم.
- **الدوران حول العمود:** لا يبدأ إلّا بعد إمساك العصا 0.7 ث، بسرعة 90°/ث. قبل ذلك، `AM_L2H_PoleReachR` و`L` (ميل للتصويب).
- **القفز الموجَّه:** `LadPoleAim` يختار عموداً في اتّجاه العصا بشرط cos > 0.8 ومسافة بين 90 و600، ثم `h = clamp(d/0.73, 250, 900)`. أمسك بعمود على بُعد 3.5 م واليدان على 3 سم.
- **القفز للخلف من الحافّة (`BackLeap`):** يدور الجسم نحو الاتّجاه ويقفز 300 أفقيّاً و380 للأعلى، كقفزة السلّم. لم يُقَس على حافّة حقيقيّة بعد.

**العارضة الأفقيّة: التنقّل يمين/يسار (`7c33b02f`):**
- **الشرط:** `BarShimTry` يعمل حين يكون التأرجح هادئاً (|θ| < 0.25 و|ω| < 0.8) والعصا الجانبيّة > 0.6.
- **الحركة:** مونتاج حركة جذر، 40–45 سم لكلّ دورة.
- **التحقّق من المكان:** `BarShimRoom` يتأكّد من وجود 45 سم من العارضة على الأقلّ.
- **وضع الحركة:** `BarUpdate` يفرض Flying، لأنّ CMC كان ينزلق إلى Falling فيسقط اللاعب.
- **قفل الجذر:** المقطعان بقفل `REF_POSE`، لأنّ الجسم كان يُرسم على بُعد 360 سم.

**الوقوف فوق العمود (`7646173c`):**
- **الصعود:**
  - الشرط: التسلّق بالعصا للأعلى عند القمّة.
  - `PTMountApply` يشغّل `AM_L2H_PoleTopUp` (ATS `idle_to_stand`، حركة جذر +43 للأمام و+173 للأعلى)، والتصادم مُطفأ.
  - بعد 2.3 ث، `PTStandApply` يثبّت الجسم على محور العمود عند z = القمّة + نصف الارتفاع، ويشغّل `PoleTopIdle`.
- **فوق العمود:** `PTPhys` يُبقي الموضع بالسرعة، ويدير الوجه نحو العصا (RInterpTo 8).
- **القفز:**
  - `PTFindTop` يختار أفضل قمّة أمامه: cos > 0.8، ومسافة 60–600، وفرق ارتفاع من −250 إلى +200.
  - مدّة الطيران `T = clamp(d/450, 0.45, 1.1)`، وقوس باليستيّ `vz = (dz + 588·T²)/T`.
  - بلا هدف، قفزة أماميّة: 450 أفقيّاً و450 للأعلى.
- **الهبوط:**
  - `PTLandFind` يمسك الهبوط على مسافة أفقيّة < 30 سم وفرق عموديّ بين −40 و+45 سم، بشرط vz < 50.
  - يشغّل `PoleTopLand`، ثم Idle قبل نهايته (عند الموضع 1.15) فلا تظهر فجوة.
- **النزول (المثلّث):** `PTDownApply` يشغّل `PoleTopDown` (ATS `stand_to_idle`، −43 و−173). بعد 1.42 ث، `LadApply` على نفس العمود، فيكمل التسلّق نزولاً.
- **الحماية:** `LadCanStart` يرفض الإمساك ما دام `IsPoleTop`، ولمدّة 0.25 ث بعد القفز.

**القياس (الوقوف فوق العمود):**
- القدمان على 601–603 فوق قمّة 600، وعلى بُعد 0–4 سم من المحور.
- القفز 350 سم بين عمودين يهبط بالضبط على القمّة، والقفزة الأخيرة قبل الهبوط 12 سم.
- القفز إلى لا شيء يسقط إلى الأرض من 6 م.
- **Listen Server:**
  - بتحكّم العميل: فرق المتوسّط 2 سم، والأقصى 40 سم (إطار واحد عند الهبوط بسبب التأخّر).
  - بتحكّم الخادم كما يراه العميل: 0.6 سم، والمقاطع نفسها.

**فخاخ:**
- **`write_graph_dsl` يربط مخرجات الدالّة بالترتيب، لا بالاسم.** `(bind (_found _fwd _kind) (LadFind ...))` أعطى `_fwd = Base` و`_kind = 0`، فتوقّف الإمساك بالعمود بصمت. اربط كلّ المخرجات بالترتيب.
- **مسار المونتاج كوسيط موضعيّ يضيع:** `(Animation|PlayAnimMontage "/Game/...")` يُكتب 0. استعمل دائماً `:AnimMontage "..."`.
- **`ANIM_FIRST_FRAME`:** يرسم الجسم عند إزاحة جذر الإطار الأوّل. حين لا يكون الجذر في الأصل، استعمل `REF_POSE`.
- **تشغيل PIE:** يكون دائماً بـ`start_pie` مع `on_compile_errors: suppress`، لأنّ أخطاء ATS في BP تفتح نافذة تمنع البدء.
- **اختيار الـsubsystem الصحيح:** المختار هو ما يعيش `LocalPlayer` الخاصّ به في نفس العالم (`s.get_outer().get_world() == W`). قاعدة `subs[-N]` فشلت حين كان PIE Standalone بعميلين، أي عالمان مستقلّان.

**مؤجّل:**
- **الميل على العمود عند اللاعبين الآخرين:** `LadSide` غير متزامن، فلا يرون وضعيّة الميل.
- **أنيميشن القفز بين القمم:** يستعمل الآن وضعيّة السقوط من GASP. مقاطع ATS `stand_jump*` متوفّرة إن أردنا قفزة مخصّصة.

## ٨٩ · الأسلحة على BP_L2H_Base: الإمساك، ووضعيّة التصويب، وخطّ الخطّاف، والمؤشّر (2026-10-07)

**«الشيء الأبيض»:** الأسلحة كانت تُلصق بجذر الجسم، لأنّ مقابس `grip_*` غير موجودة على الهيكل. البندقيّة (Fab Sci-Fi Gun الكبيرة) كانت تظهر بالعرض عند الورك، والأسلحة المغمدة كانت داخل الصدر.

**ما تغيّر:**
- **المقابس:** `grip_pistol` و`grip_hook` و`grip_rifle` و`grip_shotgun` على `SK_UEFN_Mannequin`، وأبوها `weapon_r`.
  - الدوران محسوب من `weapon_r` في وضعيّة التصويب المقيسة في PIE: الماسورة (+Y في MSHF) إلى أمام الشخصيّة، و+Z للأعلى.
  - المسدّس والخطّاف (0.98، 88.85، 4.49)، والرشّاش والبندقيّة (−0.08، 94.3، 0.59).
- **طبقة الجزء العلويّ في ABP:** مقاطع ALS (`L2H_W_*_Aim_Sweep`) محوّلة إلى UEFN.
  - التحويل جعلها Additive من نوع `RotationOffsetMeshSpace`، فانهار الجزء العلويّ. صارت `AAT_NONE`، مقطعاً مقطعاً مع الحفظ. تغيير مقطعين دفعة واحدة أسقط المحرّر أثناء الضغط.
- **`OnShotFX`:** `NS_L2H_HookBeam` للخطّاف (ليزر سماويّ 0.3 ث)، و`NS_L2H_Tracer` للرصاص.
  - نسختان بتشغيل مرّة واحدة من `NS_Laser_01`، والأصل لم يُمسّ.
  - `LaserEnd` بإحداثيّات العالم. يُنشأ بلا تفعيل، ثم يُضبط `LaserEnd`، ثم `Activate`.
- **`MulticastShotFX`:** صار Reliable في C++.
- **القدرات الأساسيّة:** `HookAbilityClass` و`TransferAbilityClass` على BP_L2H_Base. الضغطة الأولى على الخطّاف كانت «refused» لأنّ `GiveAbility` داخل قدرة الإطلاق يتأجّل. صار اللوغ «granted 7».
- **المؤشّر:** `CrosshairUpdate` بعد `WeaponAnimUpdate`، للاعب المحلّيّ فقط ما دام يحمل سلاحاً، ويستقبل `SetAimState(WpnAimOn)`.

**القياس:**
- الصور من كلّ جانب: المسدّس والخطّاف في اليد، والرشّاش والبندقيّة أخمصهما عند الكتف وماسورتهما للأمام.
- إطلاق حقيقيّ (حقن `IA_L2H_Fire`):
  - الخطّاف من المضيف: الليزر يظهر على المضيف والعميل.
  - طلقة العميل: الذخيرة على الخادم 30 → 29.
- المؤشّر: `XhairOn` صحيح على اللاعب المحلّيّ فقط في العالمين.

**فخاخ:**
- **RPC من Python لا يُعتمد عليه:** قدرة مفعّلة بـ`try_activate_ability_by_class` من Python لم توصل الـmulticast إلى العميل، والإدخال المحقون أوصله. اختبر الشبكة بالإدخال فقط.
- **`set_editor_property` على CDO الـBP من Python:** يُقرأ صحيحاً من الـCDO، لكنّ نسخة PIE بقيت None. استعمل `set_cdo_properties`، وتحقّق على نسخة PIE.
- **`Utilities|IsValid` في الـDSL:** يُحلّ إلى الماكرو الذي له مخرجا تنفيذ، فيبتلع بقيّة الدالّة. استعملت علماً منطقيّاً (`XhairOn`) بدلاً منه.
- **نافذة AimAssistSystem:** تُرسم بـSlate، فلا تستجيب للمفاتيح. والنقرة تحتاج إحداثيّات منطقيّة، لأنّ الشاشة على تكبير 150%.

**ملاحظات:**
- `NS_L2H_*` تعتمد على مواد `FreeStylizedLaserBeamVFX`، و`DA_L2H_Weapon_Hook` على مجسّم Phaser من Fab. كلاهما غير مُدرج في git.
- الشخصيّة تبدأ والمسدّس في يدها. هذا سلوك `WeaponComponent` الحاليّ.

## ٩٠ · مراجعة عمل المستخدم: الصحّة والموت والحمل والرمي والخطّاف بالجاذبية (2026-10-08)

**قبل البدء:**
- نسخة احتياطيّة من `Source/` في `Saved/Backup_UserWork_2026-10-08/`، فيها نسخة قبل المراجعة ونسخة بعدها مع الـdiff.
- المحرّر كان يعمل بـDLL أقدم من `L2H_BaseCharacter` (تعديل الساعة 07:27). Live Coding لا يطبّق تغييرات الهيدر، فلم تكن `CarryLiftMontage` موجودة. الحلّ إعادة بناء كاملة.

**أخطاء وجدتها وأصلحتها (كلّها مقيسة في PIE بشاشة مقسومة):**
1. **ميّت إلى الأبد:** بعد الرجوع إلى نقطة الحفظ يبقى `Dead=true` والصحّة 0، لأنّ `ApplyRecord` يعيد الوقت فقط.
   - **الإصلاح:** `SetDead(false)` ثم `ResetHealth`، وتُنهى كلّ عمليّات الحمل قبل أيّ شيء. وفعلت الأمر نفسه في `ResetRun`.
2. **الإحياء لا يرفع الجسد:** `SetDead(false)` لم يكن يلغي الـragdoll.
3. **موت لاعب يعيد الفريق كلّه بعد ثانيتين:** هذا يمنع الحمل والإحياء من الأساس.
   - **الآن:** الإعادة تحدث فقط حين لا يبقى أحد واقفاً (`IsAnyPlayerStanding`). هذا يشمل ميّتاً مع ساقط (downed).
4. **الإحياء:** F (TimeTransfer) يقبل الرفيق الميّت أيضاً. يعود بنصف صحّته وبوقته مضافاً إليه الهديّة، ويُنزَل عن الأكتاف أوّلاً.
5. **حمل الرفيق كان مستحيلاً:** لا `MassProfile` ولا `Grabbable` على `BP_L2H_Base`.
   - صار `FindGrabTarget` يقبل الرفيق الساقط أو الميّت، ويُحسب وزنه 50 كغ، فيكفي لاعب واحد.
   - **الجسد على الكتف:** الحوض يتبع الكبسولة والباقي ragdoll جزئيّ.
   - تدوير الـmesh لا ينفع، لأنّ عقدة Offset Root Bone تعيده واقفاً في كلّ إطار.
   - `OnRep_CarriedActor` كان يشغّل الفيزياء على كبسولة شخصيّة عند الإنزال.
6. **E لا يلتقط شيئاً عند النظر للأمام:** خطّ التتبّع من العين يمرّ فوق الصندوق.
   - **الإصلاح:** إن فشل التتبّع، يُؤخذ أقرب شيء قابل للحمل ضمن 110 سم أمام الشخصيّة.
7. **الصندوق كان على اليد اليمنى**، فيبتلع الشخصيّة وينزل حتى الأرض.
   - **الآن:** أمام الصدر، والمسافة حسب حجمه، ما لم يحدّد الجسم socket خاصّاً.
8. **الرمي ينحرف جانبياً:** الجسم كان يبدأ داخل كبسولة الرامي فيرتدّ عنها.
   - **الآن:** يُوضع أمام الصدر قبل الدفعة. صار الرمي مستقيماً: 880 سم للأمام بلا انحراف.
   - الرفيق المحمول يُنزَل ولا يُرمى.
9. **الشظيّة ترجع للضحيّة في الإطار التالي:** المغناطيس 350 سم أكبر من مسافة التناثر.
   - **الآن:** الضحيّة لا تلتقطها لمدّة 1.5 ث، والميّت لا يلتقط شيئاً.
10. **إطلاق النار على جدار يصنع شظيّة وقت مجّانيّة:** صار التناثر فقط لما له `TimeAttributeSet`.
11. **`State.Downed` لا يمنع أيّ قدرة:** لا توجد `ActivationBlockedTags` في أيّ مكان.
    - **الآن:** `ServerTryActivateAbilityBy*` يرفض الطلب من الساقط والميّت.
12. **مدى الخطّاف 600 سم من الكاميرا:** وهي خلف الشخصيّة بنحو 2 م، فالمدى الحقيقيّ كان نحو 4 م فقط. صار 2500.

**خطّاف بالجاذبية (جديد):**
- `StartHookSwing` يعلّق اللاعب بحبل طوله المسافة الحاليّة.
- كلّ إطار يُحذف فقط الجزء الخارج من السرعة، فالجاذبية تصنع التأرجح.
- الحبل يقصر بمعدّل 300 سم/ث، وأقلّ طول له 180 سم، ويُفلَت تلقائياً بعد 6 ث.
- عند الوصول أو الضغط على Q مرّة ثانية يُفلَت بدفعة 350 للأعلى.
- الحبل خطّ Niagara مستمرّ (`NS_L2H_HookRope`) من اليد إلى نقطة التعليق.
- **القياس:** من حبل طوله 10 م، نزل تحت النقطة إلى z≈280، ثم صعد إليها (z 961)، وأُفلت فوقها إلى 1111.

**تحقّقت (بلا تعديل):**
- التصويب بالضغط المستمرّ: السلاح مخفيّ قبله وظاهر أثناءه، والسرعة 200.
- الإطلاق أثناء التصويب يعمل (8→7)، ومن دون تصويب لا يطلق (7→7).
- الضرر: 30 نقطة صحّة تساوي 15 ث، والتعافي يعمل.
- ساعة الوقت وشريط الصحّة منفصلان، وشريط الأسلحة أسفل الشاشة.

**فخاخ:**
- `PlayAnimMontage` على الخادم لا يصل للعملاء (`CarryLiftMontage`).
- اختيار subsystem الإدخال في الشاشة المقسومة: `LocalPlayer` الأقلّ رقماً من الزوج الأحدث هو اللاعب 0.
- `LogVerbose` لا يُطبع حتى مع `log LogL2HTime Verbose`.
- استدعاءات Python المتتالية بلا انتظار لا تعطي PIE أيّ إطار.

## ٩١ · المسدّس الفيزيائيّ، والخطّاف على كلّ شيء، والحمل بأنيميشن الحزمة، ومنيو الوقت، وعدوّ اختبار (2026-10-09)

**المسدّس:**
- `AL2H_Projectile`: رصاصة حقيقيّة بسرعة 9000 سم/ث وبجاذبية.
- يحدّد ضرر الطلقة ما تصيبه الرصاصة فعلاً، لا ما كان تحت المؤشّر.
- الضرر يمرّ في المسارين: الوقت/الصحّة عبر director، و`ApplyPointDamage`.
- الرصاصة تدفع الأجسام الفيزيائيّة.
- تُفعَّل لكلّ سلاح بحقل `ProjectileClass` في بيانات السلاح، وهي مفعّلة الآن للمسدّس فقط.
- **القياس:** طلقة أفقيّة طارت 40 م وهبطت من z=141 إلى 28. ثلاث طلقات أسقطت الروبوت (60→40→…→0).

**الخطّاف:**
- **ما أضفتَه أنت:** مدى 60 م، والتعلّق بأيّ سطح، والسحب أثناء الضغط.
- **ما أصلحته:** لم يعد يرفض الـAnchor أو الأجسام غير الفيزيائيّة، فهو الآن يتعلّق بها.
- **سحب الأجسام:** قوس سرعة محسوب من المسافة يهبط أمام اللاعب، بدل دفعة ثابتة تقسم على الوزن (240 م/ث لجسم 5 كغ).
  - **القياس:** 8 م، والقمّة 1.7 م، ويهبط على بعد متر أمام اللاعب.
  - النسخة الأولى (14 م/ث) انزلقت 12 م بعد اللاعب.
- **«قفزة L1 الغريبة»:** L1 مربوط بـ`IA_L2H_Hook`. الكاميرا المائلة للأسفل تصيب الأرض أمام اللاعب، فيقفز 420 ثم يُسحب على الأرض.
  - **الآن:** لا تعلّق بالأرض تحت مستوى الرأس.
  - القفزة لا تحدث إلّا إن كانت النقطة أعلى من الشخصيّة.

**الحمل:**
- **الرفع والإنزال:** مقاطع من الحزمة عبر multicast بحسب نوع الحمل:
  - الخفيف: RightArm BoxWithHandle.
  - الثقيل أو الضخم: LargeBox.
  - الرفيق: Worker Start/End.
- **وضعيّة الإمساك:** طبقة كانت موجودة في ABP (CarryPose/CarryAlpha).
- **الجسم المحمول:**
  - يتبع اليدين، ويُدفع دائماً أمام الكبسولة ولا ينزل تحت الأرض. صندوق 1.2 م كان يبتلع الشخصيّة.
  - الضخم (أكبر من 35 سم) يأخذ وضعيّة LargeBox حتى لو كان خفيفاً.
- **الرفيق:** مقطع Customer بعقدة واحدة، بإزاحة 28 سم للأمام. قيس من الحزمة: يدا الحامل عند +24/115، وحوض المحمول عند 114.

**منيو الوقت:**
- يُفتح بالسهم الأيسر أو X. الأعلى والأسفل للاختيار، والأيمن للتأكيد.
- الخيارات: أعطِ، اطلب، اقبل، ارفض، إغلاق.
- **القياس:** التنقّل لم يبدّل السلاح. التأكيد أرسل الطلب، فصار `PendingRequest` عند اللاعب الثاني.

**عدوّ اختبار:** `AL2H_EnemyRobot` (Robot Scout R21):
- يطارد أقرب لاعب واقف، ويضرب 8 كلّ ثانيتين.
- صحّته 60، ويسقط ragdoll عند الموت.
- هو ممثّل C++ لا يحتاج BP، ويوضع في المستوى مباشرة.

**فخاخ:**
- المحرّر في الخلفيّة يعمل بنحو 3 إطارات/ث، والحقن يفوت.
- اسم actor مكرّر (T_Pull_Light) أعاد StaticMeshActor بدل GrabProp.
- `set_control_rotation` ثم إطلاق في نفس الاستدعاء: الكاميرا لم تتحدّث بعد.

## ٩٢ · أزرار الذراع، وخطّاف «ينطلق وحده»، وتوقيت الرفع (2026-10-09)

**الأزرار:**
- أُزيل الخطّاف من L1، وبقي على Q.
- أُزيل إعطاء الوقت من R1، وبقي على F وفي المنيو.
- منيو الوقت: `IMC_L2H_TimeMenu` يُضاف بأولويّة 100 ما دام المنيو مفتوحاً:
  - X أو المثلّث للتأكيد، والدائرة أو السهم الأيسر للإغلاق، والأسهم للتنقّل.
  - في الكيبورد: Enter أو E للتأكيد، وEsc للإغلاق.
  - هذه المفاتيح مستهلكة، فلا يقفز X ولا يتفادى الدائرة أثناء المنيو.

**«الخطّاف يتفعّل عند الحمل»:**
- `L2H_Ability_Interact` كان يحوّل أيّ شيء أثقل من 60 كغ إلى الخطّاف. والخطّاف صار يتعلّق بكلّ شيء، فضغط المثلّث على صندوق للاعبَين كان يسحب اللاعب.
- **الآن:** زرّ التفاعل يعني الحمل دائماً، وقدرة الحمل تطبّق قواعد الوزن.

**توقيت الرفع:**
- الـmulticast يحمل موضع الجسم الأصليّ.
- كلّ جهاز يُبقي الجسم مكانه حتى 45% من المقطع، ثم يرفعه إلى اليدين في 0.25 ث.
- طبقة الإمساك تبدأ بعد الإمساك فقط.
- الحركة مقفلة (`MOVE_None`) حتى 85% من المقطع على الخادم والمالك.
- **القياس:** مع دفع العصا للأمام بقي اللاعب ثابتاً. بقي الصندوق عند z=50 حتى الإطار 57 ثم صعد، وعادت الحركة عند الإطار 110.

**«حركة غريبة عند الوقوف»:**
- مع غياب الإدخال، الكبسولة ثابتة تماماً (823 إطاراً).
- المرجّح: حالة Idle Break في GASP (حركات انتظار عشوائيّة)، أو انحراف عصا الذراع.

## ٩٣ · الحمل بـIK لليدين والتفات الجذع، مؤشّر الرمي، نقطة الليزر، والخطّاف كقوس (2026-10-09)

**الحمل:**
- `UpdateCarryAnimation` يحسب `CarryTwist`: الفرق بين اتجاه الكاميرا والجسم، محصوراً بـ±70° (`CarryMaxTwist`) ومنعَّماً.
- الجسم المحمول يُوضع من الصدر (`spine_05`) أمام الجذع الملتفّ، ويدور معه.
- الحجم يُحسب من صندوق الجسم المحلّيّ، لا من الصندوق العالميّ الذي يكبر عند الدوران.
- أهداف اليدين:
  - جسم أضيق من الكتفين تمسكه اليدان من جانبيه.
  - جسم أعرض تمسكه اليدان من وجهه القريب، والمسافة بينهما 64 سم.
  - **القياس:** صندوق 1 م كان جانباه خارج مدى الذراعين (الخطأ 50 سم). صار الخطأ 0–3 سم، والجذع مستقيم أو ملتفّ 50°.
- ABP، بعد `LayeredBoneBlend_1`:
  `LocalToComponent` ← `ModifyBone spine_03` (yaw إضافيّ، مكوّن) ← `TwoBoneIK hand_r/hand_l` (هدف عالميّ، المرفق من `lowerarm`) ← `ComponentToLocal` ← Output.
  - متغيّرات: `CarryTwistRot`، `CarryIK`، `CarryHandL/R`.
- **القفز مع الحمل:** الجسم المحمول كان QueryOnly، فكان بحث الباركور يراه أمام الصدر ويأخذ ضغطة القفز كقفزة فوقه. صار بلا تصادم أثناء الحمل على كلّ الأجهزة، ويُعاد عند الإفلات. **القياس:** القفز ارتفع 32 سم مع صندوق محمول.
- `WeaponAnimUpdate`: الحمل يُحسب مشغولاً، فلا يرتفع السلاح أثناء الحمل.

**الرمي:**
- `GetThrowLaunch` معادلة واحدة لنقطة الإفلات والسرعة، تستعملها قدرة الرمي والمؤشّر معاً.
- `UpdateThrowPreview`: نقاط `PredictProjectilePath` على `ThrowArc` (instanced spheres، للمالك فقط)، وقرص عند نقطة السقوط.
- **القياس:** الصندوق سقط على بعد 48 سم من القرص، معظمها انزلاق بعد السقوط. نصف قطر التوقّع صار بحجم الجسم.

**التصويب:**
- أُزيلت علامة التصويب من `CrosshairUpdate`، وبقي شريط الأسلحة.
- `AimDot`: كرة متوهّجة عند نقطة اصطدام الليزر، للمالك فقط، يكبر حجمها قليلاً مع المسافة.

**الخطّاف:**
- السبب في أنّ اللاعب «يطير بعيداً»: الحبل القديم كان يسرّع اللاعب دون نقطة توقّف.
- **الآن:** نقطة هبوط محسوبة مسبقاً، وقوس `SuggestProjectileVelocity_CustomArc` بجاذبية العالم، وتوقّف عند الوصول (السرعة × 0.1).
  - على الجدار: 60 سم أمامه.
  - إن كانت حافّة الجدار قريبة فوق نقطة التعلّق: فوق السطح، 70 سم للداخل.
  - على سطح يواجه الأعلى: فوقه.
  - لا يهبط أبداً تحت الأرض.
- الضغطة الثانية أثناء الطيران تفلت.
- الحبل: أسطوانة متوهّجة من اليد اليمنى إلى النقطة، وكرة نابضة على نقطة التعلّق، تظهران على كلّ الأجهزة.
- **القياس:**
  - الجدار من 8 م: هبط عند (−2650، −1741)، أي 60 سم أمام الجدار، وتوقّف.
  - قرب أعلى منصّة 4.4 م: وقف فوقها (z=528)، داخلها بـ29 سم.

## ٩٤ · فحص شامل لنظامَي الحركة والسلاح قبل القفل (2026-10-09)

اختبار في PIE على `DefaultLevel`. أداة التشغيل `sc.py` تضع اللاعب في مكان محدّد، وتحقن الإدخال حسب جدول بزمن اللعبة، وتسجّل الحالة. السرعة كانت `slomo 0.25` لدقّة العيّنات.

**الحركة، وكلّها تعمل:**
- **أساسيّات:** الجري 500، العدو 700، القفز من الوقوف والجري والعدو (+75 سم)، والهبوط (`JustLanded` يعود false).
- **الدحرجة والانزلاق:**
  - الدحرجة: `AM_L2H_RollDive`.
  - الانزلاق: `AM_L2H_Slide`، من 700 إلى 300 مع خفض القرفصاء.
- **التسلّق:**
  - تسلّق بالجري على جدار 1.7 م وجدار 3.5 م، بمونتاج climb.
  - إمساك الحافّة من السقوط، ثمّ التنقّل الجانبيّ 90 سم/ث، ثمّ الصعود (`AM_L2H_ClimbUp`).
- **السلّم والعمود:** السلّم بالتفاعل (100 سم/ث)، والخروج من أعلاه (`LadderTopOut`)، ثمّ إمساك العمود تلقائيّاً.
- **العارضة والجدار:**
  - العارضة: تحتاج قفزة عدو من حافّة الهضبة. الإمساك (`AM_L2H_BarHang`) والتأرجح والإفلات يعمل.
  - الجري الجانبيّ: عبر حفرة 5 م (`AM_L2H_WallRun_R`).
  - العارضة الضيّقة: `IsOnBeam`، بسرعة 200.
- **الحمل:** الرفع، والإفلات تلقائيّاً عند الدحرجة. الهوك يمسك الحافّة أثناء الطيران (§93).

**السلاح:**
- الإطلاق بلا تصويب لا يعمل، وهذا مقصود.
- التصويب يُظهر الليزر، والنقطة تظهر على الأسطح.
- الطلقة تنقص المخزن وتطلق قذيفة.
- إعادة التلقيم تعيد المخزن إلى 8، والتبديل يعمل.
- الضرر: المسدّس 20، والبندقيّة 25. الروبوت (60) مات من طلقتين بالبندقيّة.

**أخطاء أُصلحت:**
1. **`bAutomatic` و`FireRate` لم يقرأهما أيّ كود.**
   - البندقيّة الآليّة كانت تطلق طلقة واحدة لكلّ ضغطة.
   - لم يكن هناك حدّ لسرعة الإطلاق: المسدّس (5/ث) والشوتغن (1.2/ث) كانا يطلقان بسرعة الضغط.
   - **الآن:** الحدّ مطبَّق في `UL2H_Ability_WeaponFire` على الخادم. والضغط المستمرّ يكرّر الطلب للسلاح الآليّ (`Input_AbilityHeld` مربوط على Triggered).
   - أُزيل `InputTriggerPressed` من `IA_L2H_Fire`، لأنّه كان يمنع الحدث Triggered أثناء الضغط.
   - **القياس:**
     - البندقيّة: 10 طلقات في ثانية ضغط.
     - المسدّس: ضغط مستمرّ يعطي طلقة واحدة، و4 ضغطات في 0.4 ث تعطي 2.
2. **طباعة تصحيح:** `Print String` كانت تُظهر نصّ "Example" على الشاشة 5 ث مع كلّ قفزة فوق عائق. أُزيلت.
3. **(سابقاً، §93):** `JustLanded` كان عالقاً على true، وكاميرا GASP كانت تتبدّل على أسهم الذراع والعجلة.

**ملاحظات لم تُغيَّر:**
- **قفزة بلا مهلة:** القفزة بعد مغادرة الحافّة بلحظة لا تعمل. يمكن إضافة مهلة قصيرة بعد الحافّة (coyote time).
- **قفزة ضاعت مرّة:** قفزة واحدة ضاعت عند حافّة الحفرة أثناء العدو، ولم تتكرّر في جلسة نظيفة. المرجَّح أنّها من حالة بقيت من اختبار سابق.
- **تحذيرات:**
  - `DefaultSlot` مكرّر في `ABP_L2H_Base`.
  - `RPCServer_SetInitialRef` لـDPTS يُرسَل للّاعب الثاني قبل أن يصبح له مالك عند الظهور.
- **ملفّات ATS:** أربعة ملفّات (أصوات وجسيمات التسلّق) فيها أخطاء compile، لأنّ أنواع الأسطح (Ice وWood وغيرها) غير معرَّفة في إعدادات المشروع. هذه ملفّات الحزمة، لا كودنا.
- **الروبوت:** `AL2H_EnemyRobot` غير موضوع في أيّ مستوى. يُضاف من Place Actors بالبحث عن "L2H Enemy Robot".

## ٩٥ · قفزة للخلف بالكاميرا، والعمود العموديّ بالمثلّث (2026-10-09)

**القفزة للخلف من الحافّة:**
- أثناء التعلّق، القفز يمرّ بفرع جديد قبل `NetHangJump`. إذا كانت الكاميرا مستديرة عن الجدار، أي حاصل الضرب النقطيّ بين اتّجاه الكاميرا واتّجاه الشخصيّة أقلّ من −0.5 (أكثر من 120°)، يأخذ نفس طريق L3: `t69_VariableSet_1` ثمّ Disengage ثمّ `BackLeap`. وإلّا يكمل القفز كما كان.
- L3 باقٍ كما هو.
- **القياس:**
  - من حافّة `LEDGE_TEST`، الكاميرا للخلف والعصا للأمام نسبةً للكاميرا: أفلت الحافّة، وصعد 58 سم، وهبط على بعد 1.4 م خلفها.
  - قوّة `BackLeap` المعرَّفة: 300 أفقيّاً و300 عموديّاً. الاتّجاه من العصا.

**العمود العموديّ (`BP_L2H_Pole`):**
- `LadCanStart`: الشرط صار «وُجد» AND `InteractBuffer > 0` للسلّم والعمود معاً. كان العمود يُمسك تلقائيّاً بالسقوط أو المشي نحوه.
- `LadPromptUpdate`: البرومبت يظهر لأيّ هدف موجود، لا للسلّم فقط. النصّ «Climb».
- **القياس:**
  - المشي نحو العمود بلا ضغط: مرّ دون إمساك، وظهر البرومبت قربه.
  - مع المثلّث: أمسكه (`AM_L2H_PoleClimbATS`) وصعد 80 سم/ث.

## ٩٦ · القفزة للخلف على الدائرة، وإلغاء فكرة الكاميرا (2026-10-09)

- **أُزيل فرع الكاميرا من §95:** القفز أثناء التعلّق عاد إلى `NetHangJump` كما كان، والتنقّل على الحوافّ لا يتأثّر بالكاميرا.
- **القفزة للخلف صارت على الدائرة (`IA_Dodge` Started):**
  - يمرّ الضغط على Switch بحالة DPTS. في حالات التعلّق (1، 2، 3، 4، 6): Disengage ثمّ `BackLeap`. خارجها تبقى الدحرجة كما هي (Triggered).
  - L3 لم يعد يقفز للخلف: يضع `SkipCrouchToggle` فقط.
- **خطأ في `BackLeap`:** كانت الدالّة تدير الشخصيّة أوّلاً، ثمّ تحسب اتّجاه الإطلاق من «خلف الشخصيّة» (عقدة pure تُقيَّم من جديد). بعد الدوران صار ذلك الخلف نحو الجدار، فاصطدم الإطلاق بالجدار وكانت سرعته الأفقيّة صفراً. لم يكن يعمل إلّا حين تحدّد العصا الاتّجاه.
  - **الإصلاح:** الإطلاق أوّلاً ثمّ الدوران.
- **القوّة:** 550 أفقيّاً (كانت 300) و480 عموديّاً (كانت 380).
- **القياس، من حافّة `LEDGE_TEST` بلا عصا:**
  - الدائرة: ارتفع 1 م، وهبط على بعد 6.6 م خلف الحافّة.
  - L3: بقي معلّقاً.
  - القفز: صعد إلى الأعلى (`AM_L2H_ClimbUp`).
- **حادثة:** تعديل `BP_L2H_Base` وتجميعه أثناء تشغيل PIE، وسكربتات الاختبار ممسكة بالشخصيّة، أسقط المحرّر (Access Violation في GC). القاعدة: أوقف PIE قبل أيّ تعديل على BP.

## ٩٧ · القفز المطوّل على الحافّة، وإلغاء القفزة الحرّة، والمثلّث للنزول (2026-10-09)

**الضغطة القصيرة والمطوّلة أثناء التعلّق:**
- ضغط القفز في حالات التعلّق يستدعي `BeginHangJumpPress` (C++) بدل `NetHangJump`.
- عند رفع الإصبع (`IA_Jump` Completed) تُرجع `EndHangJumpPress` قيمة true إذا كانت الضغطة قصيرة، فيُستدعى `NetHangJump` (قفزة التسلّق العاديّة).
- إذا بقي الزرّ مضغوطاً 0.3 ث (`HangHoldTime`)، يطلق Tick قفزة حرّة `HangDirectionalJump` باتّجاه العصا:
  - العصا تُقرأ من `IA_Move` عبر Enhanced Input.
  - جانبيّاً: 385 على طول الجدار. للأعلى: 335 + 265. للأسفل: إفلات. ودائماً 80 بعيداً عن الجدار.
  - بلا عصا: للأعلى.
  - تُطبَّق محلّيّاً وعلى الخادم (`ServerHangDirectionalJump`)، ثمّ يبدأ بحث الهواء فيلتقط حافّة في الطريق.
- **إلغاء القفزة الحرّة:** في `HangJump`، فرع `ProbeResult == 3` (لا هدف) صار لا يفعل شيئاً. كان يفلت الحافّة ويقفز للأعلى أو للخلف.

**القفزة للخلف (الدائرة):** خُفّضت قرابة 30%: 385 أفقيّاً و335 عموديّاً.

**المثلّث للنزول وإمساك الحافّة (وL3 باقٍ):**
- إذا لم تجد قدرة Interact شيئاً للحمل، تستدعي `ClientInteractFoundNothing`.
- في BP: الحدث `OnInteractFoundNothing`، ثمّ تُفحص حالة DPTS فلا يكمل إلّا إذا كانت NONE، ولا سلّم قريب (`LadPrompt == 0`، و`IsLadClimbing` = false). بعدها تُستدعى عقدة `NetClimbDown` منفصلة، بلا ذيل `SkipCrouchToggle` الخاصّ بـL3.
- الأولويّة: الحمل، ثمّ السلّم أو العمود، ثمّ النزول.

**القياس:**
- **المثلّث:** على حافّة `LEDGE_01` (3.5 م) نزل اللاعب (`AM_L2H_ClimbDown`) وصار معلّقاً، وبقي `SkipCrouchToggle` = false.
- **ضغط مطوّل يميناً:** قفزة (−385، −80، 335)، والتقط نفس الحافّة أبعد بنحو 1.5 م.
- **ضغطة قصيرة والعصا للأسفل (لا هدف):** بقي معلّقاً.
- **الدائرة:** 4 م للخلف (كانت 6.6 م).

## ٩٨ · إصلاح النزول بالمثلّث: الطيران، والقفزة المفاجئة، و«يعمل مرّة واحدة» (2026-10-09)

**السبب الحقيقيّ للطيران وللقفزة المفاجئة:**
- المونتاج القديم `AM_L2H_ClimbDown` يحوي نافذة Motion Warp (`AN_MotionWarp_WithOffset`) موجّهة إلى الهدف `height`.
- لا شيء في مسار النزول يضبط هذا الهدف، فيبقى فيه ما تركته DPTS. وُجد عند القياس قرب أصل العالم: (50، 0، 0).
- لذلك يبدأ الأنيميشن في الأعلى ثمّ يُسحب الجسم أمتاراً. في الاختبار سُحب من (750، −180) إلى (106، 0)، بسرعات بين 2000 و4000 سم/ث. وعند الصعود بعده يطير.

**المسار الجديد، كلّه في C++ (`TryClimbDownToLedge`):**
- المثلّث وL3 يستدعيان الدالّة نفسها.
- خطوة سريعة (0.12 ث) إلى نقطة بداية المقطع، مع الدوران ليواجه الحافّة للخارج.
- ثمّ مونتاج جديد `AM_L2H_LedgeClimbDown`، وهو المقطع نفسه بلا أيّ Notify، بحركة جذر في وضع Flying.
- حركة الجذر مصغّرة بـ`SetAnimRootMotionTranslationScale` = (HalfHeight − 13) / 141.3، فيكون مركز الكبسولة عند 0.7 ث (حين ينتهي الدوران نحو الجدار) أعلى من الحافّة بـ13 سم.
- عند 0.7 ث: يُترك الجسم (Falling)، ويُعدَّل الاتّجاه ليواجه الجدار، ويبدأ بحث الهواء.
- إمساك DPTS العاديّ (`LedgeCatch`) يكمل إلى التعلّق.
- إذا تُرك أدنى من الحافّة (جُرّب −42 سم) أو بزاوية مائلة (45°)، لا تجد DPTS الحافّة، أو تمسك بهدف خاطئ.

**«يعمل مرّة واحدة»:**
- بعد الصعود والالتفات يقف اللاعب على طرف الحافّة تماماً.
- كان فحص وجه الجدار يتوقّف عند مركز الكبسولة، ولا يصل إلى الوجه إذا كان المركز فوقه أو بعده.
- صار الفحص يمتدّ 60 سم داخل الجسم.

**القياس:**
- **على `LEDGE_TEST`:** أربع جولات متتالية، كلّ جولة مثلّث ثمّ تعلّق `LEDGE_CLIMB` عند (−168، 335) ثمّ قفز ثمّ صعود (`AM_L2H_ClimbUp`) ثمّ التفات. لا طيران، والأربع كلّها نجحت.
- **على `LEDGE_01`:** النزول يمسك الحافّة عند (−125، 276).
- **قديم وغير مرتبط:** الضغطة القصيرة للصعود من `LEDGE_01` لا تفعل شيئاً، حتّى بعد إمساك عاديّ بالسقوط. يحتاج فحصاً منفصلاً.

**ملاحظة اختبار:** بعد إعادة تشغيل المحرّر ينخفض الأداء إلى 2–10 إطارات/ث لفترة، فتضيع الضغطات المحقونة القصيرة. صرت أنتظر حتّى يعود الوقت الحقيقيّ = وقت اللعبة، وأجعل الضغطة 0.2 ث.
