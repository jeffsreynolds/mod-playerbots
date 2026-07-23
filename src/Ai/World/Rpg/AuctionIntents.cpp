/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AuctionIntents.h"

#include "ItemUsageValue.h"
#include "Playerbots.h"

namespace
{
    bool IsEligibleAuctionIntentItem(Player* bot, PlayerbotAI* botAI, Item* item)
    {
        if (!item || bot->GetItemByGuid(item->GetGUID()) != item)
            return false;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || !item->GetCount())
            return false;

        if (item->IsSoulBound() || proto->Bonding == BIND_WHEN_PICKED_UP || proto->Bonding == BIND_QUEST_ITEM)
            return false;

        if (!item->CanBeTraded() || item->IsNotEmptyBag() || proto->HasFlag(ITEM_FLAG_CONJURED) ||
            item->GetUInt32Value(ITEM_FIELD_DURATION))
            return false;

        ItemUsage usage =
            botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(item->GetEntry()))->Get();
        return usage == ITEM_USAGE_AH;
    }

    AuctionBot::AuctionIntentsValue* GetAuctionIntentsValue(PlayerbotAI* botAI)
    {
        return dynamic_cast<AuctionBot::AuctionIntentsValue*>(
            botAI->GetAiObjectContext()->GetUntypedValue("auction intents"));
    }
}  // namespace

namespace AuctionBot
{
    AuctionIntentsValue::AuctionIntentsValue(PlayerbotAI* botAI, std::string const name)
        : ManualSetValue<AuctionIntentList&>(botAI, intents, name)
    {
    }

    std::string const AuctionIntentsValue::Save()
    {
        std::ostringstream out;
        bool first = true;

        for (AuctionIntent const& intent : value)
        {
            if (!first)
                out << "^";
            else
                first = false;

            out << intent.itemGuid.GetRawValue() << "," << intent.itemEntry << "," << intent.count << ","
                << static_cast<uint32>(intent.state) << "," << static_cast<uint64>(intent.createdAt) << ","
                << static_cast<uint64>(intent.updatedAt);
        }

        return out.str();
    }

    bool AuctionIntentsValue::Load(std::string const text)
    {
        value.clear();

        for (std::string const& row : split(text, '^'))
        {
            std::vector<std::string> fields = split(row, ',');
            if (fields.size() != 6)
                continue;

            try
            {
                ObjectGuid itemGuid(static_cast<uint64>(std::stoull(fields[0])));
                if (!itemGuid.IsItem())
                    continue;

                AuctionIntent intent;
                intent.itemGuid = itemGuid;
                intent.itemEntry = static_cast<uint32>(std::stoul(fields[1]));
                intent.count = static_cast<uint32>(std::stoul(fields[2]));
                intent.state = static_cast<AuctionIntentState>(std::stoul(fields[3]));
                intent.createdAt = static_cast<time_t>(std::stoull(fields[4]));
                intent.updatedAt = static_cast<time_t>(std::stoull(fields[5]));
                value.push_back(intent);
            }
            catch (std::exception const&)
            {
                continue;
            }
        }

        return true;
    }

    void AuctionIntentsValue::Reconcile()
    {
        std::vector<Item*> items =
            AI_VALUE2(std::vector<Item*>, "inventory items", "usage " + std::to_string(ITEM_USAGE_AH));
        std::unordered_set<ObjectGuid> eligibleGuids;
        uint32 now = static_cast<uint32>(time(nullptr));

        for (Item* item : items)
        {
            if (!IsEligibleAuctionIntentItem(bot, botAI, item))
                continue;

            eligibleGuids.insert(item->GetGUID());

            auto existing = std::find_if(value.begin(), value.end(), [item](AuctionIntent const& intent) {
                return intent.itemGuid == item->GetGUID();
            });

            if (existing == value.end())
            {
                value.push_back({item->GetGUID(), item->GetEntry(), item->GetCount(), AuctionIntentState::Pending,
                                 static_cast<time_t>(now), static_cast<time_t>(now)});
                continue;
            }

            existing->itemEntry = item->GetEntry();
            existing->count = item->GetCount();
            existing->updatedAt = now;
            if (existing->state == AuctionIntentState::Ignore || existing->state == AuctionIntentState::Posted)
                existing->state = AuctionIntentState::Pending;
        }

        value.erase(std::remove_if(value.begin(), value.end(), [&eligibleGuids](AuctionIntent const& intent) {
                        return eligibleGuids.find(intent.itemGuid) == eligibleGuids.end();
                    }),
                    value.end());
    }

    AuctionIntentList& ReconcileAuctionIntents(PlayerbotAI* botAI)
    {
        AuctionIntentsValue* intents = GetAuctionIntentsValue(botAI);
        if (!intents)
        {
            static AuctionIntentList emptyIntents;
            return emptyIntents;
        }

        intents->Reconcile();
        return intents->Get();
    }

    bool HasPendingAuctionItems(PlayerbotAI* botAI)
    {
        AuctionIntentList& intents = ReconcileAuctionIntents(botAI);
        return std::any_of(intents.begin(), intents.end(), [](AuctionIntent const& intent) {
            return intent.state == AuctionIntentState::Pending || intent.state == AuctionIntentState::Scheduled ||
                   intent.state == AuctionIntentState::Traveling;
        });
    }

    std::vector<Item*> GetPendingAuctionItems(PlayerbotAI* botAI)
    {
        AuctionIntentList& intents = ReconcileAuctionIntents(botAI);
        std::vector<Item*> items;

        for (AuctionIntent const& intent : intents)
        {
            if (intent.state != AuctionIntentState::Pending && intent.state != AuctionIntentState::Scheduled &&
                intent.state != AuctionIntentState::Traveling)
                continue;

            Item* item = botAI->GetBot()->GetItemByGuid(intent.itemGuid);
            if (item && item->GetEntry() == intent.itemEntry)
                items.push_back(item);
        }

        return items;
    }

    void SetAuctionIntentState(PlayerbotAI* botAI, ObjectGuid itemGuid, AuctionIntentState state)
    {
        AuctionIntentsValue* intentsValue = GetAuctionIntentsValue(botAI);
        if (!intentsValue)
            return;

        AuctionIntentList& intents = intentsValue->Get();
        uint32 now = static_cast<uint32>(time(nullptr));

        for (AuctionIntent& intent : intents)
        {
            if (intent.itemGuid != itemGuid)
                continue;

            intent.state = state;
            intent.updatedAt = now;
            return;
        }
    }

    void RemoveAuctionIntent(PlayerbotAI* botAI, ObjectGuid itemGuid)
    {
        AuctionIntentsValue* intentsValue = GetAuctionIntentsValue(botAI);
        if (!intentsValue)
            return;

        AuctionIntentList& intents = intentsValue->Get();
        intents.erase(std::remove_if(intents.begin(), intents.end(), [itemGuid](AuctionIntent const& intent) {
                          return intent.itemGuid == itemGuid;
                      }),
                      intents.end());
    }
}  // namespace AuctionBot
