# دليل بناء الأنظمة في L2H — الطريقة، والوسوم، والقنوات، والمجلّدات

آخر تحديث: 2026-10-04.

هذا الملفّ مرجع **كيف** نبني الأشياء. أمّا **ماذا** بُني فموجود في `docs/L2H_STATUS_AND_ROADMAP.md`، وتفاصيل كلّ بند في `docs/L2H_LEDGE_BUILD_LOG.md`.

---

## 1. الخريطة العامّة

| الطبقة | أين | ماذا |
|---|---|---|
| **الحركة** | `Content/_Project/Characters/BP_L2H_Base` (أصله `Character`) + `ABP_L2H_Base` | GASP (Motion Matching) + DPTS (الحواف والعوارض) + حركاتنا: الدحرجة، التزحلق، الجري على الجدار، التأرجح، السلّم، العمود، الهبوط بدحرجة |
| **اللعب** (من Aura) | `Source/lastTwoHours/` + `Content/L2HTime/` | الوقت بديلاً عن الصحّة (GAS)، والتفاعل، والحقيبة، والأسلحة، والضرر، والحفظ |
| **الربط بين الطبقتين** | `BP_L2H_Base` يحمل `UL2H_InteractionComponent` (منذ 2026-10-04) | زرّ التفاعل. **باقي الأنظمة تنتظر توحيد الشخصيّة** (§6) |

**أصول البائعين لا تُعدَّل في مكانها:** GASP وDPTS وATS وMSHF. نسخ ما نحتاجه إلى `_Project` ونعدّل النسخة.

---

## 2. الوسوم (Actor Tags) — على الممثّل، لا على المكوّن

| الوسم | من يقرؤه | المعنى | ملاحظات |
|---|---|---|---|
| `climb` | DPTS | **بوّابة:** بدونه لا يُتسلَّق الممثّل أصلاً | يُضاف مع أحد الأنواع الأربعة التالية |
| `ledge` · `point` · `free` · `beam` | DPTS | نوع التسلّق | الأولويّة عند التعارض: ledge ثم point ثم free ثم beam |
| `wallrun` | DPTS | **محجوز:** يشغّل جري DPTS المدمج (طيران بسرعة ثابتة) | **لا تستعمله** لجرينا الجانبيّ |
| `siderun` | `SideRunFindWall` | جدار للجري الجانبيّ (It Takes Two) | الجدار عموديّ، وسطحه بميل أقلّ من ±0.3 |
| `swingbar` | `BarFind` | عمود أفقيّ للتأرجح | المحور على Y المحلّيّ للممثّل |
| `ladder` | `LadFind` / `LadFindTop` | سلّم | الوجه الأماميّ للممثّل X+ هو اتّجاه المتسلّق؛ والدرجات كلّ 29 سم بدءاً من 12 |
| `pole` | `LadFind` | عمود عموديّ | الإمساك به تلقائيّ |

**قاعدة:** كلّ وسم جديد يُضاف إلى هذا الجدول في نفس اليوم.

## 3. وسوم اللعب (Gameplay Tags) — `Config/DefaultGameplayTags.ini`

| المجموعة | الوسوم | الاستعمال |
|---|---|---|
| `Ability.*` | `Carry` · `ChronoBall` · `Hook` · `TimeBend` · `TimeTransfer` · `Inventory.Toggle` · `Weapon.{Aim,Fire,Reload,Switch}` | وسوم أصول القدرات؛ الإدخال يفعّلها بالوسم |
| `Input.Ability.*` | `Carry` · `ChronoBall` · `Hook` · `TimeBend` · `TimeTransfer` | ربط الإدخال بالقدرة (`FL2H_AbilityInputBinding`) |
| `State.*` | `Downed` · `Invulnerable` · `Invulnerable.Dodge` | حالات. نافذة الحصانة أثناء الدحرجة تضع `Invulnerable.Dodge` |
| `Data.Time.*` | `Amount` · `Drain` | قيم SetByCaller لتأثيرات الوقت |
| `Foley.Event.*` | خطوات وقفز وهبوط وتزحلق… | أصوات حركة GASP |
| `MotionMatching.*` · `SmartObject.*` · `StateTree.*` | من GASP | لا تُمسّ |

**مقترح للتنظيم:** وسم لكلّ حالة حركة (`State.Movement.{Climbing,Ladder,Pole,BarSwing,SideRun,Sliding,Rolling}`)، بدل الاعتماد على متغيّرات BP منفصلة. بذلك تستطيع القدرات والأعداء والواجهة السؤال عن الحالة بطريقة واحدة. انظر خارطة الطريق.

## 4. قنوات التصادم وملفّاتها — `Config/DefaultEngine.ini`

| القناة | الرقم | الاستعمال |
|---|---|---|
| `Traversable` | GameTraceChannel1 | تتبّع الاجتياز (GASP/DPTS) |
| `Mouse` | 2 | — |
| `Obstacle` | 3 | — |
| `Interact` | 4 (أُضيف 2026-10-04) | تتبّع التفاعل. **الافتراضيّ Ignore**، فلا شيء يُرى إلّا ما عليه الملفّ أدناه |

**ملفّ `L2H_Interactable`:** QueryOnly، ويحجب `Interact` و`Visibility`، ويتجاهل `Pawn` و`Camera`. ضعه على كلّ ما يُضغط عليه.
- ⚠ الإطار MSHF يستعمل القناة 1 للتفاعل. لا تنسخ عقد تتبّع من بلوبرنتاته كما هي.

## 5. المجلّدات

```
Content/_Project/
  Characters/         BP_L2H_Base · ABP_L2H_Base · Choosers/ · Test/ (نُسخ تجريبيّة)
  Movement/
    Climb/            BP_L2H_Ladder · BP_L2H_Pole · L2H_Ladder*/L2H_PoleUp · AM_L2H_Ladder*/Pole*
    BarSwing/         BP_L2H_SwingBar · AM_L2H_BarHang
    WallRun/          BP_L2H_SideRunCourse · AM_L2H_WallRun_L/R
    Dodge/            L2H_RM_Roll_* · AM_L2H_Roll/Backstep/LandRoll
  Parkour/            مونتاجات DPTS/ATS المشتقّة
  UI/                 WBP_L2H_InteractPrompt
Content/L2HTime/      أنظمة اللعب (Aura): Weapons · Items · Interaction · Damage · Abilities · Input · UI
Source/lastTwoHours/  Ability · Interaction · Item · Weapon · Damage · Save · Time · UI · Character · Grab · Ball · World
```

---

## 6. كيف تُبنى حركة جديدة (النمط المعتمد)

النمط مأخوذ من التأرجح والسلّم والجري على الجدار، وكلّها تتبعه.

1. **الممثّل في العالم:** بلوبرنت صغير بوسم (§2)، **بلا تصادم** إن كان الجسم يمرّ من خلاله.
2. **الكشف:** `XFind(Tol)` يمرّ على `GetAllActorsWithTag`، ويحسب العمق والجانب والارتفاع، ثم يرجع `Found` وبيانات الإمساك. لا تتبّع إن كفى الحساب.
3. **التحديث:** `XUpdate` يُستدعى من `UpdateMovement_PreCMC`. الترتيب الحاليّ:
   `SlideUpdate` ثم `SideRunUpdate` ثم `BarUpdate` ثم `LadUpdate`.
4. **الحالة:** متغيّر `IsX` ومعه بيانات الإمساك، ومهلة `XCooldown` تمنع الإمساك من جديد فوراً.
5. **الفيزياء:** وضع `Flying`، والسرعة = (الموضع الهدف − الموضع الحاليّ) / dt، مع حدّ أقصى.
   - يُطفأ `OrientRotationToMovement` و`UseControllerDesiredRotation` **كلّ إطار**، لأنّ GASP يعيد تشغيلهما في `UpdateRotation_PreCMC`.
   - إن عاد وضع الحركة إلى Falling، يُعاد إلى Flying.
6. **الأنيميشن:**
   - مقطع بلا حركة جذر، وموضع المونتاج = `Fraction(المسافة / طول الدورة)`، فتبقى اليدان على الدرجات صعوداً ونزولاً.
   - **يُحصر الموضع داخل المقطع**، لأنّ الوصول إلى نهايته يُنهي المونتاج.
   - **يُعاد تشغيل المونتاج إن توقّف.**
7. **ABP:** أيّ حركة تدير الممثّل أو المِش مباشرة تُضاف إلى `IsBarSwinging = char.IsBarSwinging OR char.IsLadClimbing` في EventGraph الخاصّ بالـABP. هذا يتجاوز `Offset Root Bone` ويطفئ AO والميل.
8. **الشبكة (Listen Server، الأحداث بلا معاملات):**
   ```
   Local:   XBegin  -> XApply (محلّيّاً فوراً) -> Server_XStart (عميل) | Multicast_XStart (مضيف)
   Server:  XServerStart -> XFind(Tol أوسع) -> XApply -> Multicast_XStart
   Remote:  XRemoteStart (ليس مالكاً ولا خادماً) -> متغيّرات + مونتاج فقط
   End:     XEnd / Server_XEnd / Multicast_XEnd -> XRemoteEnd
   ```
   الفيزياء تجري عند المالك والخادم معاً. الأنيميشن يجري عند الجميع.
9. **الإدخال:** القفز يمرّ عبر `SideRunJumpGate`، فإن كانت حركة جارية تعالج القفز. التفاعل يمرّ عبر `InteractPressed`.

### سلسلة المقاطع (من حزمة إلى هيكلنا)

```
batch_retarget (RTG_Manny_to_UEFN أو RTG_ALS_to_UEFN، بعد حذف الهدف، وبلا overwrite)
 -> root_fix   (نقل حركة الحوض الأفقيّة إلى الجذر إن كان الجذر فارغاً)
 -> ikbake     (ik_foot_* = FK)
 -> curves     disableleftfootik / disablerightfootik / disablepelvisadjustment = 1 طوال المقطع
 -> create_montage_from_sections -> save -> reload_packages   (وإلّا يبقى طول المونتاج 0)
```
السكربتات في مجلّد المسوّدة الخاصّ بالجلسة: `ikbake_lib.py` و`root_fix.py` و`climb_assets.py`.

### الاختبار

- **أداة القياس:** بايثون في المحرّر عبر `register_slate_post_tick_callback`، يحقن الإدخال، ويسجّل كلّ إطار الموضع والحالة والمونتاج والفجوة بين اليد والهدف.
  - أدوات موجودة: `lad.py` (سلّم وعمود)، و`lnet.py` (شبكة)، و`wt.py` (أسلحة وتفاعل)، و`bar.py`، و`wr.py`.
- **اختيار نظام الإدخال الصحيح:** اللاعب 0 هو `subs[-N]`، حيث N عدد اللاعبين المحلّيّين في العالم. Listen Server بلاعبين يضع لاعبَين في عالم الخادم.
- **قبل النقل الفوريّ (teleport)** في أداة الاختبار: يُنهى التسلّق عند **العميل والخادم معاً**.
- **المحرّر في الخلفيّة يعمل بـ3 إطارات/ث:** `bThrottleCPUWhenNotForeground = false` أثناء الاختبار.
- **دائماً:** أوقف PIE قبل أيّ تعديل أو حفظ. اقرأ ما كتبته (`read_graph_dsl`). تحقّق أنّ الملفّ تغيّر في `Content/`. ثم commit محلّيّ لكلّ بند مقيس.

### فخاخ كاتب DSL (مُثبتة)

- `else` يبدأ بـ`if` يتحوّل إلى `elif`، فتُبتلع العبارات التي تليه. لا تبدأ `else` بـ`if`.
- `if` داخل `then` بلا `else` ينهي الدالّة عند الفشل ولا يكمل. سطّحها بـ`and` و`select`.
- AND في BP لا يتوقّف عند أوّل false، فاستدعاء على ممثّل فارغ يعطي `Accessed None`. احمِه بـ`select(hit, HitActor, GameState)`.
- `Utilities|IsValid` يُحلّ إلى الماكرو، فينكسر التدفّق. استعمل متغيّراً منطقيّاً بدلاً منه.
- `FormatText` لا يقبل معاملات متغيّرة. ابنِ النصّ بـ`Append` ثم `ToText(String)`.
- استدعاءات الدوالّ الذاتيّة بمعاملات بالاسم (`:Tol 40.0`). الأحداث المخصّصة تُستدعى بـ`CallFunction|MulticastX` بعد compile.
- `vector+vector` غير موجود في DSL. اجمع المركّبات بـ`MakeVector`.
