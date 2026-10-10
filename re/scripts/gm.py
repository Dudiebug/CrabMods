"""Tiny client for the ghidra-mcp headless REST server + our annotation table."""
import json, sys, urllib.request, urllib.parse
URL = "http://127.0.0.1:8089"
def post(path, **body):
    req = urllib.request.Request(URL + path, data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    return urllib.request.urlopen(req, timeout=300).read().decode()
def get(path, **q):
    return urllib.request.urlopen(URL + path + "?" + urllib.parse.urlencode(q), timeout=300).read().decode()
def name(addr, n): return post("/rename_function_by_address", function_address=addr, new_name=n)
def proto(addr, p): return post("/set_function_prototype", function_address=addr, prototype=p)
def decomp(addr): return get("/decompile_function", address=addr)

# address -> (name, prototype or None)
ANN = {
 "0x140d2df60": ("InvArr_GetBuff", "float InvArr_GetBuff(void* Arr, char Type)"),
 "0x140d2e050": ("InvArr_GetLevel", "byte InvArr_GetLevel(void* Arr, char Type)"),
 "0x140d23b90": ("ACrabPS_HasPerk", "bool ACrabPS_HasPerk(void* PS, char PerkType)"),
 "0x140d252d0": ("ACrabPS_GetTotalPerkLevels", "int ACrabPS_GetTotalPerkLevels(void* PS)"),
 "0x140d25450": ("ACrabPS_GetPerkBaseDebuff", "float ACrabPS_GetPerkBaseDebuff(void* PS, char PerkType)"),
 "0x140d276e0": ("ACrabPS_UpdatePerkBuff", "void ACrabPS_UpdatePerkBuff(void* PS, char PerkType, bool bApply)"),
 "0x140d20f80": ("ACrabPS_AddPerkAccumulatedBuff", "void ACrabPS_AddPerkAccumulatedBuff(void* PS, char PerkType, float Amount)"),
 "0x140d22e90": ("ACrabPS_GetCollectorBuff", "float ACrabPS_GetCollectorBuff(void* PS)"),
 "0x140d23700": ("ACrabPS_GetDamageBuff", "float ACrabPS_GetDamageBuff(void* PS)"),
 "0x140d1fd60": ("ACrabPS_AddAbilityMod", "void ACrabPS_AddAbilityMod(void* PS, void* Mod)"),
 "0x140d20140": ("ACrabPS_AddMeleeMod", "void ACrabPS_AddMeleeMod(void* PS, void* Mod)"),
 "0x140d20520": ("ACrabPS_AddPerk", "void ACrabPS_AddPerk(void* PS, void* Perk)"),
 "0x140d210a0": ("ACrabPS_AddWeaponMod", "void ACrabPS_AddWeaponMod(void* PS, void* Mod)"),
 "0x140d20d00": ("ACrabPS_AddCrystals", "void ACrabPS_AddCrystals(void* PS, int Amount)"),
 "0x140d20df0": ("ACrabPS_AddKeys", "void ACrabPS_AddKeys(void* PS, int Amount)"),
 "0x140d21020": ("ACrabPS_AddScaleMultiplier", "void ACrabPS_AddScaleMultiplier(void* PS, float Amount)"),
 "0x140d168d0": ("ACrabPlayerC_GivePickup", "void ACrabPlayerC_GivePickup(void* PlayerC, void* PickupInfo)"),
 "0x140d26c00": ("ACrabPS_AddElimination", "void ACrabPS_AddElimination(void* PS)"),
 "0x140d27340": ("ACrabPS_OnRep_Eliminations", None),
 "0x140d27390": ("ACrabPS_OnRep_Inventory", None),
 "0x140d7b720": ("ACrabPC_ClientOnPickedUpPickup", None),
 "0x140d7b300": ("ACrabPC_ClientInventoryEvent", None),
 "0x140d80420": ("ACrabPS_StaticClass", None),
 "0x140d7d2a0": ("ACrabPlayerC_StaticClass", None),
 "0x140d7af40": ("ACrabPC_StaticClass", None),
 "0x1410d10f0": ("UObject_ProcessEvent", "void UObject_ProcessEvent(void* This, void* Function, void* Parms)"),
 "0x140ed4980": ("FNamePool_ctor", None),
 "0x14078da30": ("FMulticastDelegate_Broadcast0", None),
 "0x140840fd0": ("TArray28_ResizeGrow", None),
 "0x140cc7220": ("FCrabInventoryInfo_Copy", None),
 "0x140df0dc0": ("FMemory_Free", None),
}
if __name__ == "__main__":
    if sys.argv[1] == "apply":
        for a, (n, p) in ANN.items():
            r1 = name(a, n); r2 = proto(a, p) if p else ""
            if "error" in (r1 + r2).lower(): print(a, n, r1[:120], r2[:120])
    elif sys.argv[1] == "d":
        for a in sys.argv[2:]: print(decomp(a))
