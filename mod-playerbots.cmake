# Ensure auction feature sources are linked when building modules as static.
# Some build environments miss these files during source collection,
# which causes undefined vtable/linker symbols for auction classes.

if (NOT TARGET modules)
  return()
endif()

set(MOD_PLAYERBOTS_AUCTION_SOURCES
  ${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Ai/Base/Actions/AuctionHouseActions.cpp
  ${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Ai/Base/Trigger/AuctionTriggers.cpp
  ${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Ai/World/Rpg/AuctionIntents.cpp
  ${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Ai/World/Rpg/AuctionValues.cpp
  ${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Ai/World/Rpg/Strategy/AuctionStrategy.cpp
)

get_target_property(MODULES_TARGET_SOURCES modules SOURCES)
if (NOT MODULES_TARGET_SOURCES)
  set(MODULES_TARGET_SOURCES "")
endif()

set(MOD_PLAYERBOTS_MISSING_AUCTION_SOURCES)
foreach(MOD_PLAYERBOTS_SOURCE IN LISTS MOD_PLAYERBOTS_AUCTION_SOURCES)
  list(FIND MODULES_TARGET_SOURCES ${MOD_PLAYERBOTS_SOURCE} MOD_PLAYERBOTS_SOURCE_INDEX)
  if (MOD_PLAYERBOTS_SOURCE_INDEX EQUAL -1)
    list(APPEND MOD_PLAYERBOTS_MISSING_AUCTION_SOURCES ${MOD_PLAYERBOTS_SOURCE})
  endif()
endforeach()

if (MOD_PLAYERBOTS_MISSING_AUCTION_SOURCES)
  target_sources(modules PRIVATE ${MOD_PLAYERBOTS_MISSING_AUCTION_SOURCES})
endif()
