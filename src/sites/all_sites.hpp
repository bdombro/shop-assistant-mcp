#pragma once

#include "core/adapter.hpp"
#include "adverts.hpp"
#include "amazon.hpp"
#include "bing.hpp"
#include "brooks.hpp"
#include "chadwicks.hpp"
#include "currys.hpp"
#include "diy.hpp"
#include "donedeal.hpp"
#include "duckduckgo.hpp"
#include "ebay.hpp"
#include "houseoffraser.hpp"
#include "huntoffice.hpp"
#include "mcmahons.hpp"
#include "mcquillantools.hpp"
#include "mydiy.hpp"
#include "powercity.hpp"
#include "screwfix.hpp"
#include "tjomahony.hpp"
#include "tooldepotireland.hpp"
#include "toolsidee.hpp"
#include "very.hpp"
#include "woodies.hpp"

namespace shop::sites {

inline void register_all_sites(core::AdapterRegistry& reg) {
    reg.registerAdapter(std::make_unique<AdvertsAdapter>());
    reg.registerAdapter(std::make_unique<AmazonAdapter>());
    reg.registerAdapter(std::make_unique<BingAdapter>());
    reg.registerAdapter(std::make_unique<BrooksAdapter>());
    reg.registerAdapter(std::make_unique<ChadwicksAdapter>());
    reg.registerAdapter(std::make_unique<CurrysAdapter>());
    reg.registerAdapter(std::make_unique<DiyAdapter>());
    reg.registerAdapter(std::make_unique<DonedealAdapter>());
    reg.registerAdapter(std::make_unique<DuckDuckGoAdapter>());
    reg.registerAdapter(std::make_unique<EbayAdapter>());
    reg.registerAdapter(std::make_unique<HouseOfFraserAdapter>());
    reg.registerAdapter(std::make_unique<HuntOfficeAdapter>());
    reg.registerAdapter(std::make_unique<McMahonsAdapter>());
    reg.registerAdapter(std::make_unique<McquillanToolsAdapter>());
    reg.registerAdapter(std::make_unique<MydiyAdapter>());
    reg.registerAdapter(std::make_unique<PowercityAdapter>());
    reg.registerAdapter(std::make_unique<ScrewfixAdapter>());
    reg.registerAdapter(std::make_unique<TjOMahonyAdapter>());
    reg.registerAdapter(std::make_unique<ToolDepotIrelandAdapter>());
    reg.registerAdapter(std::make_unique<ToolsideeAdapter>());
    reg.registerAdapter(std::make_unique<VeryAdapter>());
    reg.registerAdapter(std::make_unique<WoodiesAdapter>());
}

} // namespace shop::sites
