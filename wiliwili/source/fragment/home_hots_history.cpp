//
// Created by fang on 2022/7/7.
//

#include <borealis.hpp>
#include "activity/player_activity.hpp"
#include "fragment/home_hots_history.hpp"
#include "view/video_card.hpp"
#include "view/recycling_grid.hpp"

class DataSourceHotsHistoryVideoList : public RecyclingGridDataSource {
public:
    DataSourceHotsHistoryVideoList(bilibili::HotsHistoryVideoListResult result)
        : videoList(result) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler,
                                  size_t index) override {
        //从缓存列表中取出 或者 新生成一个表单项
        RecyclingGridItemVideoCard* item =
            (RecyclingGridItemVideoCard*)recycler->dequeueReusableCell("Cell");

        bilibili::HotsHistoryVideoResult& r = this->videoList[index];
        brls::Logger::debug("title: {}", r.title);
        item->setCard(r.pic + "@672w_378h_1c.jpg", r.title, r.owner.name,
                      r.pubdate, r.stat.view, r.stat.danmaku, r.duration);
        item->setAchievement(r.achievement);
        return item;
    }

    size_t getItemCount() override { return videoList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        brls::Application::pushActivity(
            new PlayerActivity(videoList[index].bvid));
    }

    void appendData(const bilibili::HotsHistoryVideoListResult& data) {
        brls::Logger::error("DataSourceRecommendVideoList: append data: {}",
                            data.size());
        this->videoList.insert(this->videoList.end(), data.begin(), data.end());
    }

    void clearData() override { this->videoList.clear(); }

private:
    bilibili::HotsHistoryVideoListResult videoList;
};

HomeHotsHistory::HomeHotsHistory() {
    this->inflateFromXMLRes("xml/fragment/home_hots_history.xml");
    brls::Logger::debug("Fragment HomeHotsHistory: create");
    this->recyclingGrid->registerCell(
        "Cell", []() { return RecyclingGridItemVideoCard::create(); });
    this->requestData();
}

void HomeHotsHistory::onCreate() {
    this->registerTabAction("刷新", brls::ControllerButton::BUTTON_X,
                            [this](brls::View* view) -> bool {
                                AutoTabFrame::focus2Sidebar(this);
                                this->recyclingGrid->showSkeleton();
                                this->requestData();
                                return true;
                            });
}

brls::View* HomeHotsHistory::create() { return new HomeHotsHistory(); }

void HomeHotsHistory::onHotsHistoryList(
    const bilibili::HotsHistoryVideoListResult& result,
    const std::string& explain) {
    brls::Threading::sync([this, result, explain]() {
        recyclingGrid->setDataSource(
            new DataSourceHotsHistoryVideoList(result));
        this->labelExplain->setText(explain);
    });
}

HomeHotsHistory::~HomeHotsHistory() {
    brls::Logger::debug("Fragment HomeHotsHistoryActivity: delete");
}

void HomeHotsHistory::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}