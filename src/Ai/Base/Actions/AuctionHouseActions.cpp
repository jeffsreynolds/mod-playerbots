/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AuctionHouseActions.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "AuctionHouseMgr.h"
#include "AuctionIntents.h"
#include "AuctionValues.h"
#include "ItemUsageValue.h"
#include "Opcodes.h"
#include "Playerbots.h"

#if __has_include("AuctionHouseBotConfig.h") && __has_include("AuctionHouseBotCommon.h")
#include "AuctionHouseBotConfig.h"
#include "AuctionHouseBotCommon.h"
#define PLAYERBOTS_HAS_AHBOT_PRICING 1
#else
#define PLAYERBOTS_HAS_AHBOT_PRICING 0
#endif

namespace
{
    constexpr uint32 AUCTION_DURATION_MINUTES = 24 * 60;

    struct AuctionListingPrice
    {
        uint32 bid = 0;
        uint32 buyout = 0;
    };

    bool IsAuctionActivityLoggingEnabled()
    {
        return sPlayerbotAIConfig.logAuctionHouseActivity;
    }

    Creature* FindNearbyAuctioneer(Player* bot, PlayerbotAI* botAI)
    {
        GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();

        for (ObjectGuid const& guid : npcs)
        {
            Creature* auctioneer = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_AUCTIONEER);
            if (auctioneer && auctioneer->IsInWorld() && auctioneer->IsAlive())
                return auctioneer;
        }

        return nullptr;
    }

    bool CanStillAuction(Player* bot, PlayerbotAI* botAI, Item* item)
    {
        if (!item || bot->GetItemByGuid(item->GetGUID()) != item)
            return false;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || !item->GetCount() || item->GetCount() > 1000)
            return false;

        if (item->IsSoulBound() || proto->Bonding == BIND_WHEN_PICKED_UP || proto->Bonding == BIND_QUEST_ITEM)
            return false;

        if (!item->CanBeTraded() || item->IsNotEmptyBag() || proto->HasFlag(ITEM_FLAG_CONJURED) ||
            item->GetUInt32Value(ITEM_FIELD_DURATION) || sAuctionMgr->GetAItem(item->GetGUID()))
            return false;

        ItemUsage usage =
            botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(item->GetEntry()))->Get();
        return usage == ITEM_USAGE_AH;
    }

    uint32 CalculateAuctionBuyout(Item* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        uint64 vendorValue = uint64(proto->SellPrice) * item->GetCount();
        uint64 buyValue = uint64(proto->BuyPrice) * item->GetCount();
        uint64 fallbackValue = std::max(vendorValue * 4, buyValue);
        uint32 priceMin = sPlayerbotAIConfig.auctionPriceMinPercent;
        uint32 priceMax = std::max(priceMin, sPlayerbotAIConfig.auctionPriceMaxPercent);
        uint64 buyout = fallbackValue * urand(priceMin, priceMax) / 100;

        return static_cast<uint32>(std::clamp<uint64>(buyout, 1, MAX_MONEY_AMOUNT));
    }

    AuctionListingPrice GetPlayerbotFallbackListingPrice(Item* item)
    {
        uint32 buyout = CalculateAuctionBuyout(item);
        uint32 bid = std::max<uint32>(1, static_cast<uint32>(uint64(buyout) * 80 / 100));

        return AuctionListingPrice{ bid, buyout };
    }

#if PLAYERBOTS_HAS_AHBOT_PRICING
    AHBConfig* GetAhBotConfigForAuctioneer(Creature* auctioneer)
    {
        if (!auctioneer)
            return nullptr;

        AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(auctioneer->GetFaction());
        if (!ahEntry)
            return gNeutralConfig;

        if (AuctionHouseId(ahEntry->houseId) == AuctionHouseId::Alliance)
            return gAllianceConfig;

        if (AuctionHouseId(ahEntry->houseId) == AuctionHouseId::Horde)
            return gHordeConfig;

        return gNeutralConfig;
    }

    std::optional<AuctionListingPrice> GetAhBotListingPrice(Creature* auctioneer, Item* item)
    {
        if (!auctioneer || !item)
            return std::nullopt;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || !item->GetCount() || proto->Quality > AHB_MAX_QUALITY)
            return std::nullopt;

        AHBConfig* config = GetAhBotConfigForAuctioneer(auctioneer);
        if (!config)
            return std::nullopt;

        uint64 perUnitBuyout = config->GetItemPrice(item->GetEntry());

        if (perUnitBuyout == 0)
        {
            uint64 basePrice = config->UseBuyPriceForSeller ? proto->BuyPrice : proto->SellPrice;
            perUnitBuyout =
                basePrice * urand(config->GetMinPrice(proto->Quality), config->GetMaxPrice(proto->Quality)) / 100;
        }

        if (perUnitBuyout == 0)
            return std::nullopt;

        uint64 buyout = std::clamp<uint64>(perUnitBuyout * item->GetCount(), 1, MAX_MONEY_AMOUNT);

        uint64 bid =
            buyout * urand(config->GetMinBidPrice(proto->Quality), config->GetMaxBidPrice(proto->Quality)) / 100;
        bid = std::clamp<uint64>(bid, 1, buyout);

        return AuctionListingPrice{ static_cast<uint32>(bid), static_cast<uint32>(buyout) };
    }
#endif

    bool PostItemAtAuctionHouse(Player* bot, Creature* auctioneer, Item* item)
    {
        if (!bot || !auctioneer || !item)
            return false;

        ObjectGuid itemGuid = item->GetGUID();
        uint32 count = item->GetCount();
        AuctionListingPrice listingPrice = GetPlayerbotFallbackListingPrice(item);
        char const* pricingSource = "fallback";

#if PLAYERBOTS_HAS_AHBOT_PRICING
        if (std::optional<AuctionListingPrice> ahBotPrice = GetAhBotListingPrice(auctioneer, item))
        {
            listingPrice = *ahBotPrice;
            pricingSource = "ahbot";
        }
#endif

        uint32 bid = listingPrice.bid;
        uint32 buyout = listingPrice.buyout;

        if (!bid || !buyout || bid > buyout || buyout > MAX_MONEY_AMOUNT)
        {
            if (IsAuctionActivityLoggingEnabled())
                LOG_DEBUG("playerbots.auction",
                          "[SELL][PLACE_SKIPPED] bot={} item={} guid={} count={} invalid price bid={} buyout={} source={}",
                          bot->GetName(), item->GetEntry(), itemGuid.ToString(), count, bid, buyout, pricingSource);
            return false;
        }

        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots.auction",
                      "[SELL][PLACE_ATTEMPT] bot={} item={} guid={} count={} bid={} buyout={} source={} auctioneer={}",
                      bot->GetName(), item->GetEntry(), itemGuid.ToString(), count, bid, buyout, pricingSource,
                      auctioneer->GetEntry());

        WorldPacket packet(CMSG_AUCTION_SELL_ITEM);
        packet << auctioneer->GetGUID();
        packet << uint32(1);
        packet << itemGuid;
        packet << count;
        packet << bid;
        packet << buyout;
        packet << AUCTION_DURATION_MINUTES;

        bot->GetSession()->HandleAuctionSellItem(packet);
        bool placed = bot->GetItemByGuid(itemGuid) == nullptr && sAuctionMgr->GetAItem(itemGuid) != nullptr;

        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots.auction",
                      "[SELL][PLACE_RESULT] bot={} item={} guid={} placed={} bid={} buyout={} source={}",
                      bot->GetName(), item->GetEntry(), itemGuid.ToString(), placed ? 1 : 0, bid, buyout,
                      pricingSource);

        return placed;
    }

    uint32 GetBuyerSpendCapPercent(uint8 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_NORMAL:
                return sPlayerbotAIConfig.auctionBuyMaxSpendWhite;
            case ITEM_QUALITY_UNCOMMON:
                return sPlayerbotAIConfig.auctionBuyMaxSpendGreen;
            case ITEM_QUALITY_RARE:
                return sPlayerbotAIConfig.auctionBuyMaxSpendBlue;
            case ITEM_QUALITY_EPIC:
                return sPlayerbotAIConfig.auctionBuyMaxSpendPurple;
            case ITEM_QUALITY_LEGENDARY:
                return sPlayerbotAIConfig.auctionBuyMaxSpendOrange;
            case ITEM_QUALITY_ARTIFACT:
                return sPlayerbotAIConfig.auctionBuyMaxSpendYellow;
            default:
                return 0;
        }
    }

#if PLAYERBOTS_HAS_AHBOT_PRICING
    uint64 GetAhBotReferenceBuyerPrice(Creature* auctioneer, Item* item)
    {
        if (!auctioneer || !item)
            return 0;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || !item->GetCount() || proto->Quality > AHB_MAX_QUALITY)
            return 0;

        AHBConfig* config = GetAhBotConfigForAuctioneer(auctioneer);
        if (!config)
            return 0;

        uint64 perUnitMarketPrice = config->GetItemPrice(item->GetEntry());
        if (perUnitMarketPrice > 0)
            return std::clamp<uint64>(perUnitMarketPrice * item->GetCount(), 1, MAX_MONEY_AMOUNT);

        uint64 basePrice = config->UseBuyPriceForBuyer ? proto->BuyPrice : proto->SellPrice;
        uint64 minPercent = config->GetMinPrice(proto->Quality);
        uint64 maxPercent = std::max<uint64>(minPercent, config->GetMaxPrice(proto->Quality));
        uint64 medianPercent = (minPercent + maxPercent) / 2;
        uint64 fallback = basePrice * item->GetCount() * medianPercent / 100;
        return std::clamp<uint64>(fallback, 1, MAX_MONEY_AMOUNT);
    }
#endif

    uint64 GetReferenceBuyerPrice(Creature* auctioneer, Item* item)
    {
#if PLAYERBOTS_HAS_AHBOT_PRICING
        uint64 ahBotReference = GetAhBotReferenceBuyerPrice(auctioneer, item);
        if (ahBotReference > 0)
            return ahBotReference;
#endif

        if (!item)
            return 0;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return 0;

        return std::max<uint64>(1, CalculateAuctionBuyout(item));
    }

    bool IsUpgradeUsage(ItemUsage usage)
    {
        return usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BROKEN_EQUIP;
    }

    bool PlaceAuctionBid(Player* bot, Creature* auctioneer, uint32 auctionId, uint32 bidAmount)
    {
        if (!bot || !auctioneer || !auctionId || !bidAmount)
            return false;

        WorldPacket packet(CMSG_AUCTION_PLACE_BID);
        packet << auctioneer->GetGUID();
        packet << auctionId;
        packet << bidAmount;

        bot->GetSession()->HandleAuctionPlaceBid(packet);
        return true;
    }
}

bool ChooseAuctionHouseTargetAction::Execute(Event /*event*/)
{
    TravelTarget* oldTarget = context->GetValue<TravelTarget*>("travel target")->Get();
    if (!oldTarget)
        return false;

    TravelTarget newTarget(botAI);
    TravelMgr::NpcLocation auctioneer;

    if (!TravelMgr::instance().SelectAuctioneerByMap(bot, auctioneer))
        return false;

    if (IsAuctionActivityLoggingEnabled())
        LOG_DEBUG("playerbots.auction", "[VISIT][SCHEDULED] bot={} next target auctioneerEntry={} map={}",
                  bot->GetName(), auctioneer.entry, auctioneer.loc.GetMapId());

    WorldPosition auctioneerPosition;
    auctioneerPosition.set(auctioneer.loc);
    TravelDestination* destination = nullptr;
    WorldPosition* point = nullptr;
    float bestDistance = std::numeric_limits<float>::max();

    for (TravelDestination* candidate : TravelMgr::instance().getRpgTravelDestinations(bot, true, true))
    {
        if (candidate->getEntry() != static_cast<int32>(auctioneer.entry))
            continue;

        WorldPosition* candidatePoint = candidate->nearestPoint(&auctioneerPosition);
        if (!candidatePoint)
            continue;

        float distance = candidatePoint->fDist(&auctioneerPosition);
        if (candidatePoint->GetMapId() != auctioneerPosition.GetMapId())
            distance += 100000.0f;

        if (distance >= bestDistance)
            continue;

        destination = candidate;
        point = candidatePoint;
        bestDistance = distance;
    }

    if (!destination || !point)
        return false;

    newTarget.setTarget(destination, point);
    newTarget.setForced(true);
    setNewTarget(&newTarget, oldTarget);

    for (Item* item : AuctionBot::GetPendingAuctionItems(botAI))
        AuctionBot::SetAuctionIntentState(botAI, item->GetGUID(), AuctionBot::AuctionIntentState::Traveling);

    return true;
}

bool ChooseAuctionHouseTargetAction::isUseful()
{
    if (!botAI->AllowActivity(TRAVEL_ACTIVITY))
        return false;

    if (bot->IsInCombat())
        return false;

    if (!AI_VALUE(bool, "should visit auction house"))
        return false;

    return FindNearbyAuctioneer(bot, botAI) == nullptr;
}

bool AuctionPendingItemsAction::isUseful()
{
    return AuctionBot::HasPendingAuctionItems(botAI) && FindNearbyAuctioneer(bot, botAI) != nullptr;
}

bool AuctionPendingItemsAction::isPossible()
{
    if (bot->IsInCombat())
        return false;

    if (!WorldPosition(bot).isOverworld())
        return false;

    return FindNearbyAuctioneer(bot, botAI) != nullptr;
}

bool AuctionPendingItemsAction::Execute(Event /*event*/)
{
    Creature* auctioneer = FindNearbyAuctioneer(bot, botAI);
    if (!auctioneer)
        return false;

    if (IsAuctionActivityLoggingEnabled())
        LOG_DEBUG("playerbots.auction", "[SELL][VISIT_EXECUTED] bot={} auctioneer={} pendingItems={}",
                  bot->GetName(), auctioneer->GetEntry(), AuctionBot::GetPendingAuctionItems(botAI).size());

    std::vector<Item*> items = AuctionBot::GetPendingAuctionItems(botAI);
    uint32 posted = 0;

    for (Item* item : items)
    {
        if (posted >= std::max<uint32>(1, sPlayerbotAIConfig.auctionItemsPerVisit))
            break;

        if (!CanStillAuction(bot, botAI, item))
        {
            if (IsAuctionActivityLoggingEnabled())
                LOG_DEBUG("playerbots.auction", "[SELL][PLACE_SKIPPED] bot={} item={} guid={} reason=failed_revalidation",
                          bot->GetName(), item ? item->GetEntry() : 0,
                          item ? item->GetGUID().ToString() : ObjectGuid::Empty.ToString());
            AuctionBot::RemoveAuctionIntent(botAI, item->GetGUID());
            continue;
        }

        AuctionBot::SetAuctionIntentState(botAI, item->GetGUID(), AuctionBot::AuctionIntentState::Posting);

        if (!PostItemAtAuctionHouse(bot, auctioneer, item))
        {
            AuctionBot::SetAuctionIntentState(botAI, item->GetGUID(), AuctionBot::AuctionIntentState::Pending);
            continue;
        }

        AuctionBot::SetAuctionIntentState(botAI, item->GetGUID(), AuctionBot::AuctionIntentState::Posted);
        ++posted;
    }

    if (!posted)
    {
        AuctionBot::MarkVisitAttempt(bot);
        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots", "Bot {} could not post any of {} pending AH items at auctioneer {}",
                      bot->GetName(), items.size(), auctioneer->GetEntry());
        return false;
    }

    bool hasPendingItems = AuctionBot::HasPendingAuctionItems(botAI);
    AuctionBot::MarkVisitComplete(bot, hasPendingItems);

    if (IsAuctionActivityLoggingEnabled())
        LOG_INFO("playerbots", "Bot {} posted {} auction item(s) at auctioneer {}",
                 bot->GetName(), posted, auctioneer->GetEntry());

    botAI->SetNextCheckDelay(sPlayerbotAIConfig.rpgDelay);
    return true;
}

bool AuctionBuyUpgradesAction::isUseful()
{
    if (!sPlayerbotAIConfig.auctionBuyerEnabled)
        return false;

    return FindNearbyAuctioneer(bot, botAI) != nullptr;
}

bool AuctionBuyUpgradesAction::isPossible()
{
    if (!sPlayerbotAIConfig.auctionBuyerEnabled)
        return false;

    if (bot->IsInCombat())
        return false;

    if (!WorldPosition(bot).isOverworld())
        return false;

    return FindNearbyAuctioneer(bot, botAI) != nullptr;
}

bool AuctionBuyUpgradesAction::Execute(Event /*event*/)
{
    Creature* auctioneer = FindNearbyAuctioneer(bot, botAI);
    if (!auctioneer)
        return false;

    if (IsAuctionActivityLoggingEnabled())
        LOG_DEBUG("playerbots.auction", "[BUY][VISIT_EXECUTED] bot={} auctioneer={}",
                  bot->GetName(), auctioneer->GetEntry());

    AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(auctioneer->GetFaction());
    if (!ahEntry)
        return false;

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(auctioneer->GetFaction());
    if (!auctionHouse)
        return false;

    ItemUpgradeValue upgradeEvaluator(botAI);
    uint32 buyLimit = std::max<uint32>(1, sPlayerbotAIConfig.auctionBuyItemsPerVisit);
    uint64 maxWindowPercent = std::max<uint32>(sPlayerbotAIConfig.auctionBuyPriceMinPercent,
                                                sPlayerbotAIConfig.auctionBuyPriceMaxPercent);
    uint64 minWindowPercent = std::min<uint32>(sPlayerbotAIConfig.auctionBuyPriceMinPercent,
                                                sPlayerbotAIConfig.auctionBuyPriceMaxPercent);

    struct AuctionBuyCandidate
    {
        uint32 auctionId = 0;
        uint32 offerPrice = 0;
        bool buyout = false;
        uint8 quality = ITEM_QUALITY_POOR;
    };

    std::vector<AuctionBuyCandidate> candidates;
    candidates.reserve(64);

    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctionsBegin();
         itr != auctionHouse->GetAuctionsEnd(); ++itr)
    {
        AuctionEntry* auction = itr->second;
        if (!auction)
            continue;

        if (auction->owner == bot->GetGUID())
            continue;

        if (auction->bidder == bot->GetGUID())
            continue;

        Item* auctionItem = sAuctionMgr->GetAItem(auction->item_guid);
        if (!auctionItem)
            continue;

        ItemTemplate const* proto = auctionItem->GetTemplate();
        if (!proto)
            continue;

        if (proto->Bonding == BIND_WHEN_PICKED_UP || proto->Bonding == BIND_QUEST_ITEM)
            continue;

        if (GetBuyerSpendCapPercent(proto->Quality) == 0)
            continue;

        ItemUsage usage = upgradeEvaluator.CalculateWithThreshold(proto->ItemId, auctionItem->GetItemRandomPropertyId(),
                                                                  sPlayerbotAIConfig.buyUpgradeThreshold);

        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots.auction",
                      "[BUY][CONSIDER] bot={} auctionId={} item={} guid={} usage={} quality={} bid={} buyout={}",
                      bot->GetName(), auction->Id, proto->ItemId, auctionItem->GetGUID().ToString(),
                      static_cast<uint32>(usage), static_cast<uint32>(proto->Quality),
                      static_cast<uint32>(auction->bid ? auction->bid : auction->startbid), auction->buyout);

        if (!IsUpgradeUsage(usage))
            continue;

        uint64 referencePrice = GetReferenceBuyerPrice(auctioneer, auctionItem);
        if (!referencePrice)
            continue;

        uint64 minAcceptable = std::max<uint64>(1, referencePrice * minWindowPercent / 100);
        uint64 maxAcceptable = std::max<uint64>(1, referencePrice * maxWindowPercent / 100);

        uint64 currentBid = auction->bid ? auction->bid : auction->startbid;
        uint64 minimumBid = auction->bid ? currentBid + auction->GetAuctionOutBid() : currentBid;

        if (!minimumBid)
            continue;

        uint64 buyoutPrice = auction->buyout;
        bool canBuyout = buyoutPrice > 0 && buyoutPrice >= minAcceptable && buyoutPrice <= maxAcceptable;
        bool canBid = minimumBid >= minAcceptable && minimumBid <= maxAcceptable;

        if (!canBid && !canBuyout)
            continue;

        if (canBuyout && minimumBid >= buyoutPrice)
            canBid = false;

        uint64 offerPrice = 0;
        bool useBuyout = false;

        if (canBuyout && (!canBid || buyoutPrice <= minimumBid))
        {
            useBuyout = true;
            offerPrice = buyoutPrice;
        }
        else
        {
            offerPrice = minimumBid;
        }

        if (!offerPrice || offerPrice > MAX_MONEY_AMOUNT)
            continue;

        candidates.push_back(AuctionBuyCandidate{
            auction->Id,
            static_cast<uint32>(offerPrice),
            useBuyout,
            static_cast<uint8>(proto->Quality)
        });
    }

    if (candidates.empty())
        return false;

    std::sort(candidates.begin(), candidates.end(), [](AuctionBuyCandidate const& left, AuctionBuyCandidate const& right)
    {
        return left.offerPrice < right.offerPrice;
    });

    uint32 boughtCount = 0;
    for (AuctionBuyCandidate const& candidate : candidates)
    {
        if (boughtCount >= buyLimit)
            break;

        AuctionEntry* liveAuction = auctionHouse->GetAuction(candidate.auctionId);
        if (!liveAuction)
            continue;

        uint32 spendCapPercent = GetBuyerSpendCapPercent(candidate.quality);
        if (!spendCapPercent)
            continue;

        uint64 money = bot->GetMoney();
        uint64 rarityCap = money * spendCapPercent / 100;
        if (!rarityCap || candidate.offerPrice > rarityCap || candidate.offerPrice > money)
            continue;

        uint32 offerPrice = candidate.offerPrice;
        if (candidate.buyout && liveAuction->buyout > 0)
            offerPrice = std::min<uint32>(offerPrice, liveAuction->buyout);

        if (!offerPrice)
            continue;

        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots.auction", "[BUY][BID_ATTEMPT] bot={} auctionId={} offer={} mode={}",
                      bot->GetName(), candidate.auctionId, offerPrice, candidate.buyout ? "buyout" : "bid");

        uint64 moneyBefore = bot->GetMoney();
        if (!PlaceAuctionBid(bot, auctioneer, candidate.auctionId, offerPrice))
            continue;

        AuctionEntry* afterAuction = auctionHouse->GetAuction(candidate.auctionId);
        bool accepted = false;

        if (!afterAuction)
        {
            accepted = candidate.buyout;
        }
        else if (afterAuction->bidder == bot->GetGUID() && afterAuction->bid >= offerPrice)
        {
            accepted = true;
        }

        if (!accepted)
            continue;

        if (bot->GetMoney() >= moneyBefore)
            continue;

        if (IsAuctionActivityLoggingEnabled())
            LOG_DEBUG("playerbots.auction", "[BUY][{}] bot={} auctionId={} spent={}",
                      candidate.buyout ? "BOUGHT_OUT" : "BID_ACCEPTED", bot->GetName(), candidate.auctionId,
                      offerPrice);

        ++boughtCount;
    }

    if (!boughtCount)
        return false;

    if (IsAuctionActivityLoggingEnabled())
        LOG_INFO("playerbots", "Bot {} placed {} auction upgrade purchase(s) at auctioneer {}",
                 bot->GetName(), boughtCount, auctioneer->GetEntry());

    return true;
}
