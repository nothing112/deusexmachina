set(DEM_L3_RPG_HEADERS
	DEM/RPG/src/AI/Actions/ActionWander.h
	DEM/RPG/src/AI/ActionTpls/ActionTplWander.h
	DEM/RPG/src/AI/Goals/GoalWander.h
	DEM/RPG/src/AI/Goals/GoalWork.h
	DEM/RPG/src/AI/Memory/MemFactOverseer.h
	DEM/RPG/src/AI/Memory/MemFactSmartObj.h
	DEM/RPG/src/AI/Perceptors/PerceptorObstacle.h
	DEM/RPG/src/AI/Perceptors/PerceptorOverseer.h
	DEM/RPG/src/AI/Perceptors/PerceptorSmartObj.h
	DEM/RPG/src/AI/Sensors/SensorDamage.h
	DEM/RPG/src/AI/Sensors/SensorVision.h
	DEM/RPG/src/AI/Stimuli/StimulusSound.h
	DEM/RPG/src/AI/Stimuli/StimulusVisible.h
	DEM/RPG/src/Chr/CharacterSheet.h
	DEM/RPG/src/Combat/Dmg/Damage.h
	DEM/RPG/src/Combat/Dmg/DamageEffect.h
	DEM/RPG/src/Combat/Dmg/DamageSource.h
	DEM/RPG/src/Combat/Event/ObjDamageDone.h
	DEM/RPG/src/Combat/Prop/PropDestructible.h
	DEM/RPG/src/Combat/Prop/PropWeapon.h
	DEM/RPG/src/Dlg/DialogueManager.h
	DEM/RPG/src/Dlg/DlgContext.h
	DEM/RPG/src/Dlg/DlgGraph.h
	DEM/RPG/src/Dlg/DlgNode.h
	DEM/RPG/src/Dlg/PropTalking.h
	DEM/RPG/src/Factions/Faction.h
	DEM/RPG/src/Factions/FactionManager.h
	DEM/RPG/src/Items/Item.h
	DEM/RPG/src/Items/ItemManager.h
	DEM/RPG/src/Items/ItemStack.h
	DEM/RPG/src/Items/ItemTpl.h
	DEM/RPG/src/Items/ItemTplWeapon.h
	DEM/RPG/src/Items/Actions/ActionEquipItem.h
	DEM/RPG/src/Items/ActionTpls/ActionTplEquipItem.h
	DEM/RPG/src/Items/ActionTpls/ActionTplPickItemWorld.h
	DEM/RPG/src/Items/Prop/PropEquipment.h
	DEM/RPG/src/Items/Prop/PropInventory.h
	DEM/RPG/src/Items/Prop/PropItem.h
	DEM/RPG/src/Quests/Quest.h
	DEM/RPG/src/Quests/QuestManager.h
	DEM/RPG/src/Quests/Task.h
	DEM/RPG/src/SI/SI_L3.h
	DEM/RPG/src/World/WorldManager.h
)

set(DEM_L3_RPG_SOURCES
	DEM/RPG/src/AI/Actions/ActionWander.cpp
	DEM/RPG/src/AI/ActionTpls/ActionTplWander.cpp
	DEM/RPG/src/AI/Goals/GoalWander.cpp
	DEM/RPG/src/AI/Goals/GoalWork.cpp
	DEM/RPG/src/AI/Memory/MemFactOverseer.cpp
	DEM/RPG/src/AI/Memory/MemFactSmartObj.cpp
	DEM/RPG/src/AI/Perceptors/PerceptorObstacle.cpp
	DEM/RPG/src/AI/Perceptors/PerceptorOverseer.cpp
	DEM/RPG/src/AI/Perceptors/PerceptorSmartObj.cpp
	DEM/RPG/src/AI/Sensors/SensorVision.cpp
	DEM/RPG/src/AI/Stimuli/StimulusSound.cpp
	DEM/RPG/src/AI/Stimuli/StimulusVisible.cpp
	DEM/RPG/src/Combat/Dmg/DamageEffect.cpp
	DEM/RPG/src/Combat/Dmg/DamageSource.cpp
	DEM/RPG/src/Combat/Event/ObjDamageDone.cpp
	DEM/RPG/src/Combat/Prop/PropDestructible.cpp
	DEM/RPG/src/Combat/Prop/PropWeapon.cpp
	DEM/RPG/src/Dlg/DialogueManager.cpp
	DEM/RPG/src/Dlg/DlgContext.cpp
	DEM/RPG/src/Dlg/PropTalking.cpp
	DEM/RPG/src/Dlg/PropTalkingSI.cpp
	DEM/RPG/src/Factions/Faction.cpp
	DEM/RPG/src/Factions/FactionManager.cpp
	DEM/RPG/src/Items/Item.cpp
	DEM/RPG/src/Items/ItemManager.cpp
	DEM/RPG/src/Items/ItemTpl.cpp
	DEM/RPG/src/Items/ItemTplWeapon.cpp
	DEM/RPG/src/Items/Actions/ActionEquipItem.cpp
	DEM/RPG/src/Items/ActionTpls/ActionTplEquipItem.cpp
	DEM/RPG/src/Items/ActionTpls/ActionTplPickItemWorld.cpp
	DEM/RPG/src/Items/Prop/PropEquipment.cpp
	DEM/RPG/src/Items/Prop/PropEquipmentSI.cpp
	DEM/RPG/src/Items/Prop/PropInventory.cpp
	DEM/RPG/src/Items/Prop/PropInventorySI.cpp
	DEM/RPG/src/Items/Prop/PropItem.cpp
	DEM/RPG/src/Quests/QuestManager.cpp
	DEM/RPG/src/SI/SIDialogueManager.cpp
	DEM/RPG/src/SI/SIFaction.cpp
	DEM/RPG/src/SI/SIQuestManager.cpp
	DEM/RPG/src/World/WorldManager.cpp
)

