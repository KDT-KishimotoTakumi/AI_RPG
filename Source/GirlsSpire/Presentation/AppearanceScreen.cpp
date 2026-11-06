#include "RunScreen.h"
#include "CharacterPortraits.h"
#include "Domain/Appearance.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

void SRunScreen::AddActorPortrait(int actor,GirlsSpire::AppearanceVariant variant,const FName& id)
{
    if(actor<0 || actor>8)return;
    if(!AppearancePortraits)AppearancePortraits=MakeShared<FCharacterPortraits>();
    const auto image=AppearancePortraits->Get(actor,variant);
    if(!image){AddText(TEXT("画像を利用できません。名前と内容は確認できます。"),"appearance.image.missing",16);return;}
    Body->AddSlot().AutoHeight().HAlign(HAlign_Left)
        [SNew(SBox).WidthOverride(160).HeightOverride(240)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SImage).Image(image.Get()).AddMetaData(FDriverMetaData::Id(id))]]];
}
void SRunScreen::ShowAppearance(int actor,std::optional<GirlsSpire::AppearanceVariant> preview)
{
    using namespace GirlsSpire;
    ResetPageScroll();Body->ClearChildren();
    if(actor<0 || actor>8){
        AddText(TEXT("衣装を選ぶ"),"appearance.heading",28);
        AddText(TEXT("人物を選んで衣装を確認できます。"),"appearance.help",16);
        for(int id=0;id<=8;++id)AddAction(ActorName(id),*FString::Printf(TEXT("appearance.actor.%d"),id),[this,id]{ShowAppearance(id);});
        AddAction(TEXT("タイトルへ戻る"),"appearance.back",[this]{ShowTitle();});return;
    }
    const auto current=RetainedProfile.SelectedAppearances[actor],shown=preview.value_or(current);
    AddText(ActorName(actor),"appearance.name",28);
    AddText(current==AppearanceVariant::Standard?TEXT("現在：標準"):TEXT("現在：配色違い"),"appearance.current",16);
    AddActorPortrait(actor,shown,"appearance.image");
    AddAction(TEXT("標準を確認"),"appearance.standard",[this,actor]{ShowAppearance(actor,AppearanceVariant::Standard);});
    if(CanChooseAppearance(RetainedProfile,actor,AppearanceVariant::Alternate))
        AddAction(TEXT("配色違いを確認"),"appearance.alternate",[this,actor]{ShowAppearance(actor,AppearanceVariant::Alternate);});
    else AddText(actor==0?TEXT("配色違い：初めて勝利すると解放"):TEXT("配色違い：専用交流を終えると解放"),"appearance.locked",16);
    AddText(TEXT("新しい戦闘・会話から反映します。保存中の戦闘の衣装は変わりません。"),"appearance.timing",16);
    if(shown!=current && CanChooseAppearance(RetainedProfile,actor,shown))
        AddAction(TEXT("この衣装にする"),"appearance.confirm",[this,actor,shown]{SaveAppearance(actor,shown);});
    AddAction(shown==current?TEXT("一覧へ戻る"):TEXT("変更せず一覧へ"),"appearance.cancel",[this]{ShowAppearance();});
}
TSharedPtr<SVerticalBox> SRunScreen::AddDialoguePortrait(int actor,GirlsSpire::AppearanceVariant variant,const FName& id)
{
    TSharedPtr<SVerticalBox> portrait,content;
    Body->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[SAssignNew(portrait,SVerticalBox)]
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(content,SVerticalBox)]];
    {TGuardValue<TSharedPtr<SVerticalBox>> target(Body,portrait);AddActorPortrait(actor,variant,id);}
    return content;
}
void SRunScreen::SaveAppearance(int actor,GirlsSpire::AppearanceVariant variant)
{
    auto next=RetainedProfile;
    if(!GirlsSpire::ChooseAppearance(next,actor,variant)){ShowAppearance(actor);return;}
    if(!PersistState(StoredRun,next)){ShowSaveError([this,actor,variant]{SaveAppearance(actor,variant);});return;}
    RetainedProfile=std::move(next);ShowAppearance(actor);
}
