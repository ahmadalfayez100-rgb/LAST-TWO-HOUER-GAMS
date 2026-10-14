# ماذا نأخذ من Modern Survival Horror Framework v0.4.3

> **تحديث 2026-10-04:**
> - عمل Aura صار في git (`c4d617cb`).
> - مكوّن التفاعل لم يكن يعمل إطلاقاً لأنّه لا يُفعَّل تلقائيّاً. أُصلح، وصار على شخصيّة الحركة مع زرّ المثلّث.
> - الخطّاف كنوع سلاح موجود في C++.
>
> الحالة الكاملة وخارطة الطريق في `docs/L2H_STATUS_AND_ROADMAP.md`.

آخر فحص: 2026-10-04. المصادر:
- محتوى الإطار: `Content/ModernSurvivalHorrorFramework/`، وفيه 1281 أصلاً.
- خطّتا Aura: `Saved/.Aura/plans/iron_lantern_v1.md`، ثم `sable_quill_harbor_v2.md`، وهي المعتمدة.
- الكود: `Source/lastTwoHours/`.

---

## 1. الصورة الكبيرة: شخصيّتان منفصلتان

| | شخصيّة الحركة | شخصيّة اللعب |
|---|---|---|
| الأصل | `BP_L2H_Base` ← `Character` | `BP_L2H_PlayerCharacter` ← `AL2H_BaseCharacter` (C++) |
| الهيكل والأنيميشن | `SKM_UEFN_Mannequin` + `ABP_L2H_Base`، أي GASP وDPTS وكلّ الحركات | `SK_Mannequin` + `ThirdPerson_AnimBP`، أي حركة القالب البسيطة |
| GAS | لا يوجد ASC | ASC + `L2H_TimeAttributeSet` + قدرات الوقت |
| الإدخال | عقد BP في `IMC_Sandbox` | `SetupPlayerInputComponent` + صفوف `AbilityInputBindings` |
| المستوى | `DefaultLevel` ← `GM_L2H` | `L_TimeTest`/`L_TimeTest_Weapons` ← `GM_L2H_Time`/`GM_L2H_TestPawn` |

الأنظمة الجديدة بُنيت على `BP_L2H_TestPawn` (ابن `BP_L2H_PlayerCharacter`)، فلم تلمس شخصيّة الحركة. **لذلك لا تؤثّر على الحركة.** تحقّقتُ من ذلك بالآتي:
- لا يوجد أيّ مرجع لها في `BP_L2H_Base` أو `ABP_L2H_Base`.
- قناة `Interact` أُضيفت برقم جديد (`GameTraceChannel4`)، فبقيت القنوات `Traversable` و`Mouse` و`Obstacle` كما هي.
- وسوم `Ability.Weapon.*` و`Ability.Inventory.Toggle` إضافات فقط.

**القرار الأهمّ القادم:** اللعبة النهائيّة تحتاج شخصيّة واحدة. انظر §5.

---

## 2. ما بُني فعلاً (خطّة `sable_quill_harbor_v2`، الخطوات 1 إلى 7أ)

| النظام | الكود (C++) | الأصول | ملاحظات |
|---|---|---|---|
| التفاعل | `UL2H_InteractionComponent`: تتبّع على قناة `Interact`، و`FocusedActor` والبرومبت مُكرَّران، وRPC للضغط. `IL2H_InteractableInterface`: البرومبت، والتفاعل، وGUID، وحفظ الحالة. | `BP_L2H_Door` و`BP_L2H_Lock` و`BP_L2H_Readable` و`BP_L2H_Container` | **لا يعتمد على نوع البيدق**، فيمكن وضعه على `BP_L2H_Base` |
| الحقيبة والتخزين | `UL2H_InventoryComponent` (706 سطراً، مُكرَّر)، و`AL2H_ItemPickup`، و`AL2H_StorageActor` | 10 أصول بيانات `DA_L2H_Item_*`، و`BP_L2H_ItemPickup`، و`BP_L2H_Storage` | الواجهة بنيت في C++ فقط (`UL2H_InventoryWidget` و`SlotWidget`)، **ولا يوجد `WBP_` ابن لها بعد** |
| الأسلحة | `UL2H_WeaponComponent`: `Loadout`، و`ServerSelectWeapon`/`ServerCycleWeapon`، والذخيرة، والتصويب. و`AL2H_WeaponActor`. | `DA_L2H_Weapon_{Pipe,Pistol,Rifle,Shotgun}` و`BP_L2H_Weapon` | **التبديل بين الأسلحة موجود**. حقول المونتاجات موجودة لكنّها **فارغة** (لا مقاطع على هيكلنا) |
| القدرات | `Aim`، `WeaponFire`، `WeaponReload`، `WeaponSwitch`، `InventoryToggle` (كلّها ترث `UL2H_TimeGameplayAbility`) | `DA_L2H_AbilitySet_Weapons`، و`IA_L2H_{Fire,Aim,Reload,WeaponSwitch,Inventory}`، و`IMC_L2H_TestWeapons` | |
| الضرر | `UL2H_DamageTypeDataAsset` و`UL2H_HitReactionComponent` | `DA_L2H_Damage_{Bite,Pipe,Pistol,Rifle,Shotgun}` | الضرر **يخصم وقتاً** ولا توجد سمة Health. مجلّد الاهتزازات `/Game/L2HTime/CameraShakes` من الخطّة **غير موجود** |
| الحفظ | تعديل `UL2H_SaveSubsystem` و`L2H_SaveGame` (حالة العالم بـGUID) | | |
| بيدق الاختبار | | `BP_L2H_TestPawn` + `GM_L2H_TestPawn` + `L_TimeTest_Weapons` | فيه 4 مكوّنات: Weapon وInventory وInteraction وHitReaction |

**الحالة:**
- كلّ ما سبق **غير مُضاف إلى git** (untracked أو modified).
- آخر بناء للـDLL كان في 07:44، لكنّ `L2H_WeaponActor.cpp` عُدِّل في 10:34، **فهذا التعديل لم يُبنَ بعد**.
- الخطوة 7ب (نقل المكوّنات إلى الشخصيّة الرئيسيّة) لم تبدأ.

---

## 3. محتوى الإطار: ما المفيد؟

الرموز: ✅ أُخذ (أُعيد بناؤه C++ بنفس فكرته) · ⭐ يستحقّ الأخذ · 🟡 لاحقاً · ❌ لا نأخذه

### الأنظمة
| في الإطار | الحالة | ملاحظة |
|---|---|---|
| `AC_Interact` + `BPI_Object` + `WB_Interact` | ✅ (المنطق) / ⭐ (الواجهة) | لا توجد عندنا واجهة برومبت؛ `WB_Interact` مرجع جيّد للتصميم |
| `AC_Inventory` + `S_Item`/`S_ItemBaseData` + `DT_ItemCombine` | ✅ | الدمج من DataTable مذكور في الخطّة؛ يُتحقَّق أنّه يعمل |
| `WB_Inventory` و`WB_InventorySlot` و`WB_Shortcut` و`WB_ItemCollected` و`WB_StorageBox` و`WB_MoveToStorage` | ⭐ | تصميم جاهز لواجهة الحقيبة الناقصة (يُنسخ شكلها لا منطقها) |
| `AC_Combat` + `S_WeaponBaseData`/`S_WeaponInfo`/`S_WeaponSocket`/`S_WeaponAnimations` | ✅ | |
| `S_WeaponUpgrade` + `AC_Merchant` + `WB_Merchant*` (شراء وبيع وترقية) | 🟡 | مفيد إن أردتم تاجراً وترقية أسلحة |
| `BP_DamageType*` + `E_Damage` | ✅ | |
| `AC_Health` + `WB_Died` | ❌ | يخالف قاعدة «الوقت هو الحياة»، فالبديل `L2H_TimeDirector` |
| `AC_SaveGame` + `BP_ModernSaveGame` + `S_SaveData` | ✅ (الفكرة) | الكاتب الوحيد هو `UL2H_SaveSubsystem` |
| `BP_SaveInteract` + `WB_Save`/`WB_Load`/`WB_SaveSlot` | ⭐ | نقطة حفظ تفاعليّة مع واجهة خانات؛ عندنا `BP_L2H_CheckpointHub` |
| `BP_InspectStudio` + `WB_Inspect*` (تدوير العنصر وفحصه) | ⭐ | ميزة رعب كلاسيكيّة، وغير موجودة عندنا |
| `WB_Read`/`WB_ReadFP` + `S_FileData`/`S_Pages` (ملفّات بصفحات) | ⭐ | لدينا `AL2H_ReadableActor` بلا واجهة |
| `BP_MapSystem` + `WB_Map`/`WB_InventoryMap` + `S_ItemMapInfo` | 🟡 | خريطة تُظهر الغرف والعناصر |
| `WB_CrossHair` + `WB_WeaponData` | ⭐ | الأسلحة عندنا بلا مؤشّر تصويب ولا عرض ذخيرة |
| `WB_MainMenu`/`WB_PauseMenu`/`WB_Loading` | 🟡 | |
| `Blueprint/Camera/CameraShake` (9 اهتزازات) | ⭐ | الخطّة طلبت إعادة إنشائها ولم تُنشأ |
| `WB_BiteQTE` + `S_GrabBiteQTE` (الزومبي يمسك اللاعب) | 🟡 | يناسب التعاون: الشريك ينقذك |
| `BP_AIMaster`/`BP_AIZombie` + `AC_AIBehavior` + `BT_DemonZombie` + `BP_AISpawner` | 🟡 | الأعداء خارج النطاق الآن؛ تحتاج تكراراً شبكيّاً |
| `AC_PreCMCTick` | ✅ | `BP_L2H_Base` يستعمل نسخة منه أصلاً |

### تفاعلات العالم
| في الإطار | الحالة | ملاحظة |
|---|---|---|
| `BP_DoorBase`/`Single`/`Double` + أنيميشن الأبواب | ✅ (المنطق) / ⭐ (الأنيميشن) | `Demo/Animations/Door` يحتاج إعادة توجيه (retarget) لهيكلنا |
| `BP_ChainedLock`/`BP_CombinationLock`/`BP_PanelCode` + `WB_CombinationLock` | ✅ (المنطق) / ⭐ (واجهة الرمز) | |
| `BP_StorageBox` | ✅ | |
| `BP_InteractionDrawer`/`BP_WeaponLocker`/`BP_Barrel`/`BP_WoodenObject` | ⭐ | أمثلة جاهزة للدُّرج والخزانة والبرميل المتحطّم |
| `BP_Elevator` | ⭐ | |
| **`BP_Ladder` + `DT_ladder` + `S_Ladder` + `E_LadderInteractionPoint` + `Demo/Animations/Ladder`** | ⭐ (مرجع) | سلّم **بالتفاعل** من الأسفل والأعلى؛ انظر §4 |
| **`BP_Narrow`** (المرور الجانبيّ في الشقوق) | ⭐ | حركة جديدة تشبه It Takes Two |
| `BP_MusicPlayer`/`BP_DialogPlayer`/`BP_VideoPlayer` | 🟡 | |
| `BP_DamageTest`/`BP_HealTest`/`BP_WeaponDamageIndicator` | ⭐ (للاختبار) | |

### الأصول
| في الإطار | الحالة | ملاحظة |
|---|---|---|
| `Demo/Weapon/{Pistol,Rifle,Shotgun,Generic}` (المجسّمات) | ✅ (مستعملة في `DA_L2H_Weapon_*`) | |
| `Demo/Animations/{Pistol,Rifle,Shotgun,Melee}` | ⭐ **ناقص حرج** | الأسلحة بلا أنيميشن شخصيّة. المقاطع على هيكل UE4 وتحتاج إعادة توجيه إلى UEFN |
| `Demo/Animations/Injured` | 🟡 | مشي الجريح (عند قلّة الوقت مثلاً) |
| `Demo/Animations/ReadFile`/`Other`/`Idle` | ⭐ | |
| `Asset/Sound/*` (خطوات، سلاح، حقيبة، أبواب، زومبي، واجهة) | ⭐ | مكتبة صوت كاملة |
| `Asset/Mesh/*` (مفاتيح، أدوات، سلّم، حقيبة، آلة كاتبة…) | ⭐ | عناصر للحقيبة والمستوى |
| `Asset/Material`/`ParticleSystem` (دخان، شرر، وهج) | ⭐ | تأثيرات الإطلاق والارتطام |
| `Blueprint/Animation/MotionMatching` و`ABP_PlayerCharacterUE5` | ❌ | عندنا GASP؛ وفي النسخة مراجع مكسورة (11) |
| `BP_CharacterBase`/`BP_PlayerCharacter`/`BP_ModernHorrorGamemode` | ❌ | تفرض شخصيّتها الخاصّة؛ نأخذ أفكارها فقط |

---

## 4. السلّم بزرّ التفاعل

`BP_Ladder` في الإطار يعمل بنفس الفكرة التي طلبتها: الإمساك بالضغط، وله نقطتا دخول (`E_LadderInteractionPoint`: أعلى وأسفل).

**التصميم المقترح لسلّمنا (`BP_L2H_Ladder`):**
1. `BP_L2H_Ladder` يطبّق `IL2H_InteractableInterface`:
   - البرومبت «تسلّق».
   - صندوق تصادم بـ`L2H_Interactable` عند الأسفل، وصندوق ثانٍ عند الأعلى.
2. `L2H_Interact(Interactor)` يستدعي على الخادم دالّة في الشخصيّة، مثل `LadInteract(Ladder, FromTop)`.
3. تُزال حالات الإمساك التلقائيّ من `LadCanStart`، ويبقى الإمساك:
   - بالضغط من الأسفل (الحالة الحاليّة).
   - بالضغط من الأعلى: بداية جديدة في `LadTop` مع مقطع دخول من الأعلى. `Ladder_Climb_Down_Start` موجود عندنا في `FreeAnimationLibrary/retarget`.
   - بالضغط أثناء القفز أو السقوط قرب السلّم.
4. **يتطلّب** أن تحمل شخصيّة الحركة `UL2H_InteractionComponent` وزرّاً يستدعي `RequestInteract()`، وهو العمل الجاري الآن.
5. العمود العموديّ يبقى تلقائيّاً (القفز إليه هو الإمساك)، إلا إذا أردتم غير ذلك.

---

## 5. ما ينقص، وما أستطيع أخذه (بالأولويّة)

1. **توحيد الشخصيّة (قرار):** المكوّنات تعمل على أيّ بيدق، لكنّ **القدرات** تحتاج ASC و`TimeAttributeSet` و`TimeDirector`، وهذه تأتي من `AL2H_BaseCharacter::BeginPlay`. الخيارات:
   - **أ.** إعادة أب `BP_L2H_Base` إلى `AL2H_BaseCharacter`. هو الأنظف، لكنّه يتطلّب:
     - تعطيل `CameraBoom` و`FollowCamera` في C++ لصالح كاميرا GASP؛
     - التأكّد من ألّا يتصادم `SetupPlayerInputComponent` مع عقد الإدخال في BP؛
     - تجربة كاملة للحركة.
   - **ب.** إضافة ASC والمكوّنات إلى `BP_L2H_Base` يدويّاً، وتكرار منطق المنح. أسرع، لكنّه نسخة ثانية من المنطق.
   - توصيتي: **أ** بعد إنهاء الحركة، وفي نسخة تجريبيّة أوّلاً.
2. **أنيميشن الأسلحة:** إعادة توجيه `Demo/Animations/{Pistol,Rifle,Shotgun,Melee}` إلى UEFN، ثم بناء مونتاجات `CharacterFire`/`Reload`/`Attack`. وطبقة الجزء العلويّ للتصويب في `ABP_L2H_Base` (Layered blend per bone على `spine_01`) لتعمل مع GASP.
3. **الواجهات:** `WBP_L2H_Inventory` (ابن `UL2H_InventoryWidget`)، والبرومبت، والقراءة، ومؤشّر التصويب، وبيانات السلاح، بتصميم مأخوذ من `WB_*`.
4. **الخطّاف كسلاح:**
   - إضافة `Hook` إلى `EL2H_WeaponType` وأصل `DA_L2H_Weapon_Hook` في `Loadout`.
   - عندما يكون السلاح النشط Hook، يفعّل زرّ الإطلاق `UL2H_Ability_Hook` (موجود: سحب الأجسام أو سحب اللاعب حسب الكتلة) بدل `WeaponFire`.
   - التبديل يجري عبر `ServerCycleWeapon` الموجود.
   - الحبل والتأرجح (المؤجّلان) يُبنيان فوقه لاحقاً.
5. **اهتزازات الكاميرا والأصوات والتأثيرات:** نسخ مباشر من `Asset/` و`CameraShake`.
6. **حركات من الإطار:** `BP_Narrow` (الشقوق)، والسلّم بالتفاعل، وأنيميشن فتح الأبواب، ومشي الجريح.
7. لاحقاً: الفحص (`InspectStudio`)، والخريطة، والتاجر والترقية، والأعداء مع QTE العضّة.

**لا أستطيع** أخذ ما يلي كما هو: `AC_Health` ومسار الموت، و`ABP` الإطار، و`BP_CharacterBase` وGameMode الإطار، ومنطق `AC_*` غير المُكرَّر. نأخذ تصميمها وأصولها فقط.
