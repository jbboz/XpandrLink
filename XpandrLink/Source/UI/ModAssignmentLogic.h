/*
  ModAssignmentLogic.h
  Version: 50.0 (Pending amount for routing)
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../Data/XpanderData.h"
#include <set>
#include <map>

class ModAssignmentLogic
{
public:
    ModAssignmentLogic() = default;

    // paramID → destination index (0-46), or -1 if not a mod destination
    static int getDestinationIDForParam(int paramID)
    {
        for (const auto& [pid, did] : kDestMap)
            if (pid == paramID) return did;
        return -1;
    }

    // destination index → paramID, or -1
    static int getParamIDForDestination(int destID)
    {
        for (const auto& [pid, did] : kDestMap)
            if (did == destID) return pid;
        return -1;
    }

    void setActiveSource(int sourceIndex, const juce::String& name) {
        activeSourceIndex = sourceIndex;
        activeSourceName = name;
        isRoutingMode = true;
    }

    void assignToDestination(int paramID, MidiEngine* /*engine*/) {
        if (!isRoutingMode) return;
        int destID = getDestinationIDForParam(paramID);
        if (destID >= 0) {
            int src = activeSourceIndex;
            int amt = pendingAmount;
            // Optimistic immediate update so green LED appears without
            // waiting for the patch-dump round-trip.
            addOptimisticRoute(paramID);
            cancelRouting();
            // onModulationAssigned handler (EditorTabComponent) computes the correct
            // idSource and calls engine->addModulation with it.
            if (onModulationAssigned) onModulationAssigned(src, destID, amt);
        }
    }

    void cancelRouting() {
        isRoutingMode = false;
        activeSourceIndex = -1;
        activeSourceName = "";
    }

    void setPendingAmount(int v) { pendingAmount = juce::jlimit(-63, 63, v); }
    int  getPendingAmount()  const { return pendingAmount; }

    bool isRouting() const { return isRoutingMode; }
    juce::String getCurrentSourceName() const { return activeSourceName; }
    bool isValidModDestination(int paramID) const { return getDestinationIDForParam(paramID) >= 0; }

    // Track which paramIDs are actively present in the mod matrix
    void setActiveRoutedParamIDs(const std::set<int>& ids) { activeRoutedParamIDs = ids; }
    std::set<int> getActiveRoutedParamIDs() const { return activeRoutedParamIDs; }
    bool isActivelyRouted(int paramID) const { return activeRoutedParamIDs.count(paramID) > 0; }

    // Optimistically light a destination LED immediately after the user assigns a
    // routing, before the confirming patch dump arrives. The insert is timestamped
    // so it only survives a *stale* in-flight patch dump (see reconcileWithDecodedSet),
    // not a deliberate patch load made seconds later.
    void addOptimisticRoute(int paramID)
    {
        activeRoutedParamIDs.insert(paramID);
        pendingRouteTimes_[paramID] = juce::Time::getMillisecondCounter();
    }

    // Clear a routing's LED (e.g. user removed the mod). Erases from both the active
    // set and the optimistic-pending map so it cannot be resurrected by reconcile.
    void removeRoute(int paramID)
    {
        activeRoutedParamIDs.erase(paramID);
        pendingRouteTimes_.erase(paramID);
    }

    // Reconcile the freshly decoded routed-param set from a patch dump with any
    // still-recent optimistic inserts. The decoded set REPLACES the active set
    // (so stale LEDs from a previously loaded patch clear), except optimistic
    // inserts younger than kOptimisticRouteWindowMs are merged back in to survive
    // a patch dump that was requested before the user's assignment landed.
    void reconcileWithDecodedSet(std::set<int> decoded)
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (auto it = pendingRouteTimes_.begin(); it != pendingRouteTimes_.end(); )
        {
            if (now - it->second > kOptimisticRouteWindowMs)
                it = pendingRouteTimes_.erase(it);     // expired — let the patch dump be authoritative
            else
                { decoded.insert(it->first); ++it; }    // still in flight — keep the LED lit
        }
        activeRoutedParamIDs = std::move(decoded);
    }

    // Called when user clicks a param that is a valid mod destination (non-routing mode).
    // Fires onDestinationFocused(destID) so ModSummaryPanel can filter to that destination.
    void notifyDestinationFocused(int paramID) {
        if (isRoutingMode) return;
        int destID = getDestinationIDForParam(paramID);
        if (destID >= 0 && onDestinationFocused)
            onDestinationFocused(destID);
    }
    // Called from FullModMatrixPanel where destID is already known.
    void notifyDestinationFocusedDirect(int destID) {
        if (isRoutingMode) return;
        if (destID >= 0 && onDestinationFocused)
            onDestinationFocused(destID);
    }

    // Args: sourceIndex, destIndex (EnumModulationDestinations), amount (-63…+63)
    std::function<void(int, int, int)> onModulationAssigned;
    // Fired when a valid-destination param is clicked; arg = destID (0-46)
    std::function<void(int)> onDestinationFocused;

private:
    bool isRoutingMode = false;
    int activeSourceIndex = -1;
    juce::String activeSourceName;
    int pendingAmount = 63;
    std::set<int> activeRoutedParamIDs;
    // paramID → millisecond timestamp of an optimistic (pre-confirmation) insert
    std::map<int, juce::uint32> pendingRouteTimes_;
    // How long an optimistic LED survives an unconfirmed/stale patch dump. Long
    // enough to cover an in-flight dump + the post-assignment re-request, short
    // enough that deliberate patch navigation never inherits a prior patch's LED.
    static constexpr juce::uint32 kOptimisticRouteWindowMs = 2500;

    // paramID ↔ destID lookup table (cross-referenced against EnumModulationDestinations)
    static constexpr std::pair<int,int> kDestMap[] = {
        {0,0},  {2,1},  {3,2},                          // VCO1: Freq, PW, Vol
        {20,3}, {22,4}, {23,5},                          // VCO2: Freq, PW, Vol
        {40,6}, {41,7}, {43,8}, {44,9},                  // VCF: Freq, Res, VCA1, VCA2
        {70,10},{74,11}, {80,12},{84,13}, {90,14},{94,15},
        {100,16},{104,17}, {110,18},{114,19},             // LFO1-5: Speed, Amp
        // ENV1-5 individual params removed — ENV tabs used for destination focus instead
        {60,45}, {63,46},                                 // FM Amp, Lag Rate
    };
};