//
// Created by fang on 2022/7/10.
//

#include "bilibili.h"
#include "presenter/home_hots_weekly.hpp"

void HomeHotsWeeklyRequest::requestData() { this->requestHotsWeeklyList(); }

void HomeHotsWeeklyRequest::requestHotsWeeklyList() {
    bilibili::BilibiliClient::get_hots_weekly_list(
        [this](const bilibili::HotsWeeklyListResult& result) {
            this->onHotsWeeklyList(result);
            this->weeklyList = result;

            // 获取到列表后，自动加载最新一期
            if (result.size() > 0) {
                this->requestHotsWeeklyVideoList(result[0].number);
            }
        },
        [this](const std::string& error) { this->onError(error); });
}

void HomeHotsWeeklyRequest::requestHotsWeeklyVideoList(int number) {
    bilibili::BilibiliClient::get_hots_weekly(
        number,
        [this](const bilibili::HotsWeeklyVideoListResult& result,
               const std::string& label, const std::string& reminder) {
            this->onHotsWeeklyVideoList(result, label, reminder);
        },
        [this](const std::string& error) { this->onError(error); });
}

void HomeHotsWeeklyRequest::requestHotsWeeklyVideoListByIndex(size_t index) {
    if (index >= weeklyList.size()) return;
    this->requestHotsWeeklyVideoList(weeklyList[index].number);
}

std::vector<std::string> HomeHotsWeeklyRequest::getWeeklyList() {
    std::vector<std::string> res = {"刷新"};
    for (auto w : weeklyList) {
        res.push_back(w.name + "    " + w.subject);
    }
    return res;
}
