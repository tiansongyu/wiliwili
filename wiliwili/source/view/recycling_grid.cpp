//
// Created by fang on 2022/6/15.
//

#include "view/recycling_grid.hpp"
#include <borealis/core/time.hpp>

/// RecyclingGridItem

RecyclingGridItem::RecyclingGridItem() {
    this->setFocusable(true);
    this->registerClickAction([this](View* view) {
        RecyclingGrid* recycler =
            dynamic_cast<RecyclingGrid*>(getParent()->getParent());
        if (recycler)
            recycler->getDataSource()->onItemSelected(recycler, index);
        return true;
    });
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

size_t RecyclingGridItem::getIndex() const { return this->index; }

void RecyclingGridItem::setIndex(size_t value) { this->index = value; }

RecyclingGridItem::~RecyclingGridItem() {
    brls::Logger::debug("delete RecyclingGridItem {}", this->describe());
}

/// Skeleton cell

SkeletonCell::SkeletonCell() { this->setFocusable(false); }

RecyclingGridItem* SkeletonCell::create() { return new SkeletonCell(); }

void SkeletonCell::draw(NVGcontext* vg, float x, float y, float width,
                        float height, brls::Style style,
                        brls::FrameContext* ctx) {
    brls::Time curTime = brls::getCPUTimeUsec() / 1000;
    float p            = (curTime % 1000) * 1.0 / 1000;
    p                  = fabs(0.5 - p) + 0.25;

    NVGcolor end = background;
    end.a        = p;

    NVGpaint paint = nvgLinearGradient(vg, x, y, x + width, y + height,
                                       a(background), a(end));
    nvgBeginPath(vg);
    nvgFillPaint(vg, paint);
    nvgRoundedRect(vg, x, y, width, height, 6);
    nvgFill(vg);
}

/// Skeleton DataSource

class DataSourceSkeleton : public RecyclingGridDataSource {
public:
    DataSourceSkeleton(unsigned int n) : num(n) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) {
        SkeletonCell* item =
            (SkeletonCell*)recycler->dequeueReusableCell("Skeleton");
        item->setHeight(recycler->estimatedRowHeight);
        return item;
    }

    size_t getItemCount() { return this->num; }

    void clearData() { this->num = 0; }

private:
    unsigned int num;
};

/// RecyclingGrid

RecyclingGrid::RecyclingGrid() {
    brls::Logger::debug("View RecyclingGrid: create");

    // Create hint views
    this->hintImage = new brls::Image();
    this->hintImage->detach();
    this->hintImage->setImageFromRes("pictures/empty.png");
    this->hintLabel = new brls::Label();
    this->hintLabel->detach();
    this->hintLabel->setFontSize(14);

    this->setFocusable(false);

    this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    // Create content box
    this->contentBox = new RecyclingGridContentBox(this);
    this->setContentView(this->contentBox);

    this->registerFloatXMLAttribute("itemHeight", [this](float value) {
        this->estimatedRowHeight = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("spanCount", [this](float value) {
        if (value != 1) {
            isFlowMode = false;
        }
        this->spanCount = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("itemSpace", [this](float value) {
        this->estimatedRowSpace = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("preFetchLine", [this](float value) {
        this->preFetchLine = value;
        this->reloadData();
    });

    this->registerBoolXMLAttribute("flowMode", [this](bool value) {
        this->spanCount  = 1;
        this->isFlowMode = value;
        this->reloadData();
    });

    this->registerCell("Skeleton", []() { return SkeletonCell::create(); });
    this->showSkeleton();
}

RecyclingGrid::~RecyclingGrid() {
    brls::Logger::debug("View RecyclingGridActivity: delete");
    if (this->hintImage) this->hintImage->freeView();
    this->hintImage = nullptr;
    if (this->hintLabel) this->hintLabel->freeView();
    this->hintLabel = nullptr;
    if (this->dataSource) delete this->dataSource;
    for (auto it : queueMap) {
        for (auto item : *it.second) item->freeView();
        delete it.second;
    }
}

void RecyclingGrid::draw(NVGcontext* vg, float x, float y, float width,
                         float height, brls::Style style,
                         brls::FrameContext* ctx) {
    // 触摸或鼠标滑动时会导致屏幕元素位置变更
    // 简单地在draw函数中调用itemsRecyclingLoop 实现动态的增删元素
    // todo：只在滑动过程中调用 itemsRecyclingLoop 以节省静止时的计算消耗
    itemsRecyclingLoop();

    ScrollingFrame::draw(vg, x, y, width, height, style, ctx);

    if (!this->dataSource || this->dataSource->getItemCount() == 0) {
        if (!this->hintImage) return;
        float w1 = hintImage->getWidth(), w2 = hintLabel->getWidth();
        float h1 = hintImage->getHeight(), h2 = hintLabel->getHeight();
        this->hintImage->setAlpha(this->getAlpha());
        this->hintImage->draw(vg, x + (width - w1) / 2, y + (height - h1) / 2,
                              w1, h1, style, ctx);
        this->hintLabel->setAlpha(this->getAlpha());
        this->hintLabel->draw(vg, x + (width - w2) / 2, y + (height + h1) / 2,
                              w2, h2, style, ctx);
    }
}

void RecyclingGrid::registerCell(
    std::string identifier, std::function<RecyclingGridItem*()> allocation) {
    queueMap.insert(
        std::make_pair(identifier, new std::vector<RecyclingGridItem*>()));
    allocationMap.insert(std::make_pair(identifier, allocation));
}

void RecyclingGrid::addCellAt(size_t index, int downSide) {
    RecyclingGridItem* cell;
    //获取到一个填充好数据的cell
    cell = dataSource->cellForRow(this, index);
    cell->setWidth((renderedFrame.getWidth() - paddingLeft - paddingRight) /
                       spanCount -
                   estimatedRowSpace);
    cell->setDetachedPositionX(
        renderedFrame.getMinX() + paddingLeft +
        (renderedFrame.getWidth() - paddingLeft - paddingRight) / spanCount *
            (index % spanCount));

    float cellHeight = estimatedRowHeight;

    if (isFlowMode) {
        if (cellHeightCache[index] == -1) {
            // 没有预定义cell的高度，使用cell默认的高度
            cellHeight = cell->getHeight();
            if (cellHeight > estimatedRowHeight) {
                cellHeight = estimatedRowHeight;
            }
            cellHeightCache[index] = cellHeight;
        } else {
            // dataSource 中指定了cell的高度，使用预定义的值
            cellHeight = cellHeightCache[index];
        }
        cell->setHeight(cellHeight);

        // getHeightByCellIndex：获取当前cell的相对Y坐标 （相对于renderedFrame的顶部）
        cell->setDetachedPositionY(getHeightByCellIndex(index) + paddingTop);

        brls::Logger::debug("Add cell at: y {} height {}",
                            getHeightByCellIndex(index) + paddingTop,
                            cellHeight);
    }

    cell->setHeight(cellHeight);
    cell->setDetachedPositionY(getHeightByCellIndex(index) + paddingTop);
    cell->setIndex(index);

    this->contentBox->getChildren().insert(
        this->contentBox->getChildren().end(), cell);

    // Allocate and set parent userdata
    size_t* userdata = (size_t*)malloc(sizeof(size_t));
    *userdata        = index;

    cell->setParent(this->contentBox, userdata);

    // Layout and events
    this->contentBox->invalidate();
    cell->View::willAppear();

    if (index < visibleMin) visibleMin = index;

    if (index > visibleMax) visibleMax = index;

    // 只有元素出现在首列时才需要考虑修改 renderedFrame
    if (index % spanCount == 0) {
        if (!downSide) renderedFrame.origin.y -= cellHeight + estimatedRowSpace;

        renderedFrame.size.height += cellHeight + estimatedRowSpace;
    }

    // 瀑布流模式需要不断修正高度
    if (isFlowMode)
        contentBox->setHeight(
            getHeightByCellIndex(this->dataSource->getItemCount()) +
            paddingTop + paddingBottom);

    brls::Logger::debug("Cell #" + std::to_string(index) + " - added");
}

void RecyclingGrid::setDataSource(RecyclingGridDataSource* source) {
    if (this->dataSource) delete this->dataSource;

    this->dataSource = source;
    if (layouted) reloadData();
}

void RecyclingGrid::reloadData() {
    if (!layouted) return;

    // 将所有节点从屏幕上移除放入重复利用的列表中
    auto children = this->contentBox->getChildren();
    for (auto const& child : children) {
        queueReusableCell((RecyclingGridItem*)child);
        this->contentBox->removeView(child, false);
    }

    visibleMin = UINT_MAX;
    visibleMax = 0;

    renderedFrame            = brls::Rect();
    renderedFrame.size.width = getWidth();

    setContentOffsetY(0, false);

    if (dataSource) {
        // 设置列表的高度（真实高度，非显示的高度）
        if (!isFlowMode || spanCount != 1) {
            // 设置了固定的高度
            contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) *
                                      this->getRowCount() +
                                  paddingTop + paddingBottom);
        } else {
            // 获取每个cell的高度并缓存起来
            cellHeightCache.clear();
            for (int section = 0; section < dataSource->getItemCount();
                 section++) {
                float height = dataSource->heightForRow(this, section);
                cellHeightCache.push_back(height);
            }
            contentBox->setHeight(
                getHeightByCellIndex(dataSource->getItemCount()) + paddingTop +
                paddingBottom);
        }

        // 填充足够多的cell到屏幕上
        brls::Rect frame = getLocalFrame();
        for (auto row = 0; row < dataSource->getItemCount(); row++) {
            this->addCellAt(row, true);
            // 只关注最后一列，因为只有当前行最后一列元素添加完毕时才需要考虑要不要继续添加下一行元素
            if ((row + 1) % spanCount == 0) {
                // 计算从当前元素开始（包括）向前共 preFetchLine 行元素的高度
                if (renderedFrame.getMaxY() -
                        getHeightByCellIndex(row + 1 - preFetchLine * spanCount,
                                             row + 1) >
                    frame.getMaxY()) {
                    break;
                }
            }
        }
        selectRowAt(this->defaultCellFocus, false);
    }
}

void RecyclingGrid::notifyDataChanged() {
    // todo: 目前仅能处理data在原本的基础上增加的情况，需要考虑data减少或更换时的情况
    if (!layouted) return;

    if (dataSource) {
        if (isFlowMode) {
            for (int i = cellHeightCache.size(); i < dataSource->getItemCount();
                 i++) {
                float height = dataSource->heightForRow(this, i);
                cellHeightCache.push_back(height);
            }
            contentBox->setHeight(
                getHeightByCellIndex(this->dataSource->getItemCount()) +
                paddingTop + paddingBottom);
        } else {
            contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) *
                                      this->getRowCount() +
                                  paddingTop + paddingBottom);
        }
    }
}

void RecyclingGrid::clearData() {
    if (dataSource) {
        dataSource->clearData();
        this->reloadData();
    }
}

void RecyclingGrid::setEmpty(std::string msg) {
    this->hintImage->setImageFromRes("pictures/empty.png");
    this->hintLabel->setText(msg);
    this->clearData();
}

void RecyclingGrid::setError(std::string error) {
    this->hintImage->setImageFromRes("pictures/net_error.png");
    this->hintLabel->setText(error);
    this->clearData();
}

void RecyclingGrid::setDefaultCellFocus(size_t index) {
    this->defaultCellFocus = index;
}

size_t RecyclingGrid::getDefaultCellFocus() const {
    return this->defaultCellFocus;
}

size_t RecyclingGrid::getItemCount() {
    return this->dataSource->getItemCount();
}

size_t RecyclingGrid::getRowCount() {
    return (this->dataSource->getItemCount() - 1) / this->spanCount + 1;
}

void RecyclingGrid::onNextPage(const std::function<void()>& callback) {
    this->nextPageCallback = callback;
}

void RecyclingGrid::itemsRecyclingLoop() {
    if (!dataSource) return;

    brls::Rect visibleFrame = getVisibleFrame();

    // 上方元素自动销毁
    while (true) {
        RecyclingGridItem* minCell = nullptr;
        for (auto it : contentBox->getChildren())

            // todo: contentBox 循环时加锁？it出现过空指针报错
            if (*((size_t*)it->getParentUserData()) == visibleMin)
                minCell = (RecyclingGridItem*)it;

        // 当第一个cell的顶部 与 组件顶部的距离大于 preFetchLine 行元素的距离时结束
        if (!minCell ||
            (minCell->getDetachedPosition().y +
                 getHeightByCellIndex(
                     visibleMin + (preFetchLine + 1) * spanCount, visibleMin) >=
             visibleFrame.getMinY()))
            break;

        float cellHeight = estimatedRowHeight;
        if (isFlowMode) cellHeight = cellHeightCache[visibleMin];

        renderedFrame.origin.y += minCell->getIndex() % spanCount == 0
                                      ? cellHeight + estimatedRowSpace
                                      : 0;
        renderedFrame.size.height -= minCell->getIndex() % spanCount == 0
                                         ? cellHeight + estimatedRowSpace
                                         : 0;

        queueReusableCell(minCell);
        this->contentBox->removeView(minCell, false);

        brls::Logger::debug("Cell #" + std::to_string(visibleMin) +
                            " - destroyed");

        visibleMin++;
    }

    // 下方元素自动销毁
    while (true) {
        RecyclingGridItem* maxCell = nullptr;
        // todo: contentBox 循环时加锁？it出现过空指针报错
        for (auto it : contentBox->getChildren())
            if (*((size_t*)it->getParentUserData()) == visibleMax)
                maxCell = (RecyclingGridItem*)it;

        // 当最后一个cell的顶部 与 组件底部间的距离 小于 preFetchLine 行元素的距离时结束
        if (!maxCell ||
            (maxCell->getDetachedPosition().y -
                 getHeightByCellIndex(visibleMax,
                                      visibleMax - preFetchLine * spanCount) <=
             visibleFrame.getMaxY()))
            break;

        float cellHeight = estimatedRowHeight;
        if (isFlowMode) cellHeight = cellHeightCache[visibleMax];

        renderedFrame.size.height -= maxCell->getIndex() % spanCount == 0
                                         ? cellHeight + estimatedRowSpace
                                         : 0;

        queueReusableCell(maxCell);
        this->contentBox->removeView(maxCell, false);

        brls::Logger::debug("Cell #" + std::to_string(visibleMax) +
                            " - destroyed");

        visibleMax--;
    }

    // 上方元素自动添加
    while (visibleMin - 1 < dataSource->getItemCount()) {
        if ((visibleMin) % spanCount == 0)
            // 当 renderedFrame 顶部 与 组件顶部的距离小于 preFetchLine 行cell的距离时结束
            if (renderedFrame.getMinY() +
                    getHeightByCellIndex(visibleMin + preFetchLine * spanCount,
                                         visibleMin) <
                visibleFrame.getMinY() - paddingTop) {
                break;
            }
        addCellAt(visibleMin - 1, false);
    }

    // 下方元素自动添加
    while (visibleMax + 1 < dataSource->getItemCount()) {
        // 当即将被添加的元素为新一行的开始时结束，否则填充满一整行
        if ((visibleMax + 1) % spanCount == 0)
            // 如果 renderedFrame 底部 与 组件底部 距离超过了preFetchLine 行cell的距离时结束
            if (renderedFrame.getMaxY() -
                    getHeightByCellIndex(
                        visibleMax + 1,
                        visibleMax + 1 - preFetchLine * spanCount) >
                visibleFrame.getMaxY() - paddingBottom) {
                requestNextPage = false;  // 允许加载下一页
                break;
            }
        addCellAt(visibleMax + 1, true);
    }

    if (visibleMax + 1 >= this->getItemCount()) {
        // 只有当 requestNextPage 为false时，才可以请求下一页，避免多次重复请求
        if (!requestNextPage && this->nextPageCallback) {
            brls::Logger::error("RecyclingGrid request next page");
            this->nextPageCallback();
        }
        requestNextPage = true;
    }
}

RecyclingGridDataSource* RecyclingGrid::getDataSource() const {
    return this->dataSource;
}

void RecyclingGrid::showSkeleton(unsigned int num) {
    this->setDataSource(new DataSourceSkeleton(num));
    requestNextPage = true;  // 默认不请求下一页
}

void RecyclingGrid::selectRowAt(size_t index, bool animated) {
    this->setContentOffsetY(getHeightByCellIndex(index), animated);
    this->itemsRecyclingLoop();

    for (View* view : contentBox->getChildren()) {
        if (*((size_t*)view->getParentUserData()) == index) {
            contentBox->setLastFocusedView(view);
            break;
        }
    }
}

float RecyclingGrid::getHeightByCellIndex(int index, int start) {
    if (index < start) return 0;
    if (!isFlowMode)
        return (estimatedRowHeight + estimatedRowSpace) *
               (int)((index - start) / spanCount);

    if (cellHeightCache.size() == 0) {
        brls::Logger::error(
            "cellHeightCache.size() cannot be zero in flow mode {} {}", start,
            index);
        return 0;
    }

    if (start < 0) start = 0;
    if (index > this->cellHeightCache.size())
        index = this->cellHeightCache.size();

    float res = 0;
    for (size_t i = start; i < index && i < cellHeightCache.size(); i++) {
        if (cellHeightCache[i] != -1)
            res += cellHeightCache[i] + estimatedRowSpace;
        else
            res += estimatedRowHeight + estimatedRowSpace;
    }
    return res;
}

brls::View* RecyclingGrid::getNextCellFocus(brls::FocusDirection direction,
                                            brls::View* currentView) {
    void* parentUserData = currentView->getParentUserData();

    // Allow up and down when axis is ROW
    if ((this->contentBox->getAxis() == brls::Axis::ROW &&
         direction != brls::FocusDirection::LEFT &&
         direction != brls::FocusDirection::RIGHT)) {
        int row_offset = spanCount;
        if (direction == brls::FocusDirection::UP) row_offset = -spanCount;
        View* row_currentFocus       = nullptr;
        size_t row_currentFocusIndex = *((size_t*)parentUserData) + row_offset;

        if (row_currentFocusIndex >= this->dataSource->getItemCount()) {
            row_currentFocusIndex -= *((size_t*)parentUserData) % spanCount;
        }

        while (!row_currentFocus && row_currentFocusIndex >= 0 &&
               row_currentFocusIndex < this->dataSource->getItemCount()) {
            for (auto it : this->contentBox->getChildren()) {
                if (*((size_t*)it->getParentUserData()) ==
                    row_currentFocusIndex) {
                    row_currentFocus = it->getDefaultFocus();
                    break;
                }
            }
            row_currentFocusIndex += row_offset;
        }
        if (row_currentFocus) {
            // 按键(上或下)可以导航过去的情况
            itemsRecyclingLoop();

            return row_currentFocus;
        }
    }

    if (this->contentBox->getAxis() == brls::Axis::ROW) {
        int position = *((size_t*)parentUserData) % spanCount;
        if ((direction == brls::FocusDirection::LEFT && position == 0) ||
            (direction == brls::FocusDirection::RIGHT &&
             position == (spanCount - 1))) {
            View* next = getParentNavigationDecision(this, nullptr, direction);
            if (!next && hasParent())
                next = getParent()->getNextFocus(direction, this);
            return next;
        }
    }

    // Return nullptr immediately if focus direction mismatches the box axis (clang-format refuses to split it in multiple lines...)
    if ((this->contentBox->getAxis() == brls::Axis::ROW &&
         direction != brls::FocusDirection::LEFT &&
         direction != brls::FocusDirection::RIGHT) ||
        (this->contentBox->getAxis() == brls::Axis::COLUMN &&
         direction != brls::FocusDirection::UP &&
         direction != brls::FocusDirection::DOWN)) {
        View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent())
            next = getParent()->getNextFocus(direction, this);
        return next;
    }

    // Traverse the children
    size_t offset = 1;  // which way we are going in the children list

    if ((this->contentBox->getAxis() == brls::Axis::ROW &&
         direction == brls::FocusDirection::LEFT) ||
        (this->contentBox->getAxis() == brls::Axis::COLUMN &&
         direction == brls::FocusDirection::UP)) {
        offset = -1;
    }

    size_t currentFocusIndex = *((size_t*)parentUserData) + offset;
    View* currentFocus       = nullptr;

    while (!currentFocus && currentFocusIndex >= 0 &&
           currentFocusIndex < this->dataSource->getItemCount()) {
        for (auto it : this->contentBox->getChildren()) {
            if (*((size_t*)it->getParentUserData()) == currentFocusIndex) {
                currentFocus = it->getDefaultFocus();
                break;
            }
        }
        currentFocusIndex += offset;
    }

    currentFocus = getParentNavigationDecision(this, currentFocus, direction);
    if (!currentFocus && hasParent())
        currentFocus = getParent()->getNextFocus(direction, this);
    return currentFocus;
}

void RecyclingGrid::onLayout() {
    ScrollingFrame::onLayout();
    float width = this->getWidth();
    // check NAN
    if (width != width) return;

    this->contentBox->setWidth(width);
    if (checkWidth()) {
        brls::Logger::debug("RecyclingGrid::onLayout reloadData()");
        layouted = true;
        reloadData();
    }
}

bool RecyclingGrid::checkWidth() {
    float width = getWidth();
    if (oldWidth == -1) {
        oldWidth = width;
    }
    if ((int)oldWidth != (int)width && width != 0) {
        brls::Logger::debug("RecyclingGrid::checkWidth from {} to {}", oldWidth,
                            width);
        oldWidth = width;
        return true;
    }
    oldWidth = width;
    return false;
}

void RecyclingGrid::queueReusableCell(RecyclingGridItem* cell) {
    queueMap.at(cell->reuseIdentifier)->push_back(cell);
    cell->cacheForReuse();
}

void RecyclingGrid::setPadding(float padding) {
    this->setPadding(padding, padding, padding, padding);
}

void RecyclingGrid::setPadding(float top, float right, float bottom,
                               float left) {
    paddingTop    = top;
    paddingRight  = right;
    paddingBottom = bottom;
    paddingLeft   = left;

    this->reloadData();
}

void RecyclingGrid::setPaddingTop(float top) {
    paddingTop = top;
    this->reloadData();
}

void RecyclingGrid::setPaddingRight(float right) {
    paddingRight = right;
    this->reloadData();
}

void RecyclingGrid::setPaddingBottom(float bottom) {
    paddingBottom = bottom;
    this->reloadData();
}

void RecyclingGrid::setPaddingLeft(float left) {
    paddingLeft = left;
    this->reloadData();
}

brls::View* RecyclingGrid::create() { return new RecyclingGrid(); }

RecyclingGridItem* RecyclingGrid::dequeueReusableCell(std::string identifier) {
    brls::Logger::debug("RecyclingGrid::dequeueReusableCell: {}", identifier);
    RecyclingGridItem* cell = nullptr;
    auto it                 = queueMap.find(identifier);

    if (it != queueMap.end()) {
        std::vector<RecyclingGridItem*>* vector = it->second;
        if (!vector->empty()) {
            cell = vector->back();
            vector->pop_back();
        } else {
            cell                  = allocationMap.at(identifier)();
            cell->reuseIdentifier = identifier;
            cell->detach();
        }
    }

    cell->setHeight(brls::View::AUTO);
    if (cell) cell->prepareForReuse();

    return cell;
}

/// RecyclingGridContentBox

RecyclingGridContentBox::RecyclingGridContentBox(RecyclingGrid* recycler)
    : Box(brls::Axis::ROW), recycler(recycler) {}

brls::View* RecyclingGridContentBox::getNextFocus(
    brls::FocusDirection direction, brls::View* currentView) {
    return this->recycler->getNextCellFocus(direction, currentView);
}