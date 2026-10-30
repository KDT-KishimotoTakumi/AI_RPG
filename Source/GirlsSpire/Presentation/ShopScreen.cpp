#include "RunScreen.h"
namespace {
const TCHAR* OfferName(int offer)
{
    const TCHAR* names[]={TEXT("力の護符（攻撃＋3）"),TEXT("守りの護符（防御＋2）"),TEXT("傷薬（戦闘中にHP30回復）"),TEXT("HP全回復"),TEXT("負傷1段階治療")};
    return names[offer];
}
const TCHAR* AccessoryName(GirlsSpire::Accessory value)
{
    return value==GirlsSpire::Accessory::Power?TEXT("力の護符"):value==GirlsSpire::Accessory::Guard?TEXT("守りの護符"):TEXT("なし");
}
}
void SRunScreen::RefreshShop()
{
    using namespace GirlsSpire;
    const auto& state=CurrentRun->Snapshot();
    AddText(TEXT("購入では時間は進みません。退出で1イベント終了し、この店舗へは戻れません。"),"shop.help",15);
    AddText(ShopMessage,"shop.result",15);
    // 予告を商品一覧より上へ置き、選択後に確定内容が見えるようにする。
    if(SelectedOffer>=0) {
        const auto offer=static_cast<ShopOffer>(SelectedOffer);
        if(offer!=ShopOffer::Medicine && ShopActor<0) {
            AddText(TEXT("対象を選んでください"),"shop.select",17);
            for(int id=0;id<=8;++id) if(CurrentRun->MaxHp(id)>0)
                AddAction(ActorName(id),*FString::Printf(TEXT("shop.actor.%d"),id),[this,id]{ShopActor=id;RefreshRun();});
        } else {
            const int actor=offer==ShopOffer::Medicine?0:ShopActor;
            auto preview=*CurrentRun;
            const bool legal=preview.Purchase(state.Revision,offer,actor);
            FString label=FString::Printf(TEXT("%s / %d銭\n"),OfferName(SelectedOffer),ShopPrice(offer));
            if(legal) {
                label+=FString::Printf(TEXT("所持金%d→%d銭 / 傷薬%d→%d\n"),state.Wallet.Coins,preview.Snapshot().Wallet.Coins,state.Wallet.Medicines,preview.Snapshot().Wallet.Medicines);
                if(offer!=ShopOffer::Medicine) label+=FString::Printf(TEXT("%s：HP%d→%d / 最大HP%d→%d / 負傷%d→%d\n"),*ActorName(actor),CurrentRun->CurrentHp(actor),preview.CurrentHp(actor),CurrentRun->MaxHp(actor),preview.MaxHp(actor),state.Health[actor].Wounds,preview.Snapshot().Health[actor].Wounds);
                if(offer==ShopOffer::Power || offer==ShopOffer::Guard) label+=FString::Printf(TEXT("装備：%s→%s。旧装備は消失します。\n"),AccessoryName(state.Accessories[actor]),AccessoryName(preview.Snapshot().Accessories[actor]));
                label+=TEXT("確定後は取り消せません。");
            } else {
                const bool sold=offer==ShopOffer::Power?state.Stock.Power==0:offer==ShopOffer::Guard?state.Stock.Guard==0:offer==ShopOffer::Medicine?state.Stock.Medicines==0:false;
                label+=sold?TEXT("売切れです。"):state.Wallet.Coins<ShopPrice(offer)?TEXT("所持金不足です。"):offer==ShopOffer::Medicine?TEXT("傷薬の所持上限です。"):TEXT("回復・治療の効果がありません。");
            }
            AddText(label,"shop.preview",17);
            if(legal) AddAction(TEXT("購入を確定"),"shop.confirm",[this,revision=state.Revision,offer,actor] {
                ShopMessage=CurrentRun->Purchase(revision,offer,actor)?TEXT("購入しました。"):TEXT("状態が変わりました。選び直してください。");
                SelectedOffer=-1;ShopActor=-1;RefreshRun();
            });
        }
        AddAction(TEXT("取消（費用・時間を消費しない）"),"shop.cancel",[this]{SelectedOffer=-1;ShopActor=-1;RefreshRun();});
    }
    for(int index=0;index<5;++index) {
        const int stock=index==0?state.Stock.Power:index==1?state.Stock.Guard:state.Stock.Medicines;
        const FString remaining=index<3?FString::Printf(TEXT(" / 在庫%d"),stock):TEXT(" / 回数制限なし");
        AddAction(FString::Printf(TEXT("%s / %d銭%s"),OfferName(index),ShopPrice(static_cast<ShopOffer>(index)),*remaining),
            *FString::Printf(TEXT("shop.offer.%d"),index),[this,index]{SelectedOffer=index;ShopActor=-1;ShopMessage.Empty();RefreshRun();});
    }
    AddAction(TEXT("退出する（このイベントを終了）"),"shop.exit",[this,revision=state.Revision] {
        CurrentRun->CompleteEvent(revision);SelectedOffer=-1;ShopActor=-1;ShopMessage.Empty();RefreshRun();
    });
    ShowGrowth();
}
