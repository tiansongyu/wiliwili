//
// Created by fang on 2022/8/21.
//

#include "activity/hint_activity.hpp"
#include "view/gallery_view.hpp"

#ifdef BUILTIN_NSP
#include "nspmini.hpp"
#endif

using namespace brls::literals;

const std::string galleryItemInstallNSPXML = R"xml(
    <brls:Box
        width="100%"
        height="100%"
        axis="column"
        grow="1"
        wireframe="false"
        justifyContent="center"
        alignItems="center">

        <brls:Image
                maxWidth="90%"
                maxHeight="80%"
                image="@res/pictures/hint_wiliwili.png"
                id="gallery/image"/>
        <brls:Label
                focusable="true"
                positionType="absolute"
                positionBottom="12%"
                id="gallery/label"
                text="点击  安装桌面图标"
                fontSize="24"/>
        <brls:Label
                positionType="absolute"
                positionBottom="4%"
                id="gallery/label"
                text="@i18n/wiliwili/hint4"
                fontSize="24"/>
    </brls:Box>
)xml";

class GalleryItemInstallNSP : public GalleryItem {
public:
    GalleryItemInstallNSP() {
        this->inflateFromXMLString(galleryItemInstallNSPXML);

        button->registerClickAction([](...) -> bool {
            auto dialog = new brls::Dialog(
                "在 真实系统 安装桌面图标可能导致ban机\n请再次确认");
            dialog->addButton("hints/cancel"_i18n, []() {});
            dialog->addButton("hints/ok"_i18n, []() {
#ifdef BUILTIN_NSP
                brls::Application::blockInputs();

                mini::InstallSD("romfs:/nsp_forwarder.nsp");
                unsigned long long AppTitleID = mini::GetTitleID();
                appletRequestLaunchApplication(AppTitleID, NULL);
#endif
            });
            dialog->open();
            return true;
        });
    }

private:
    BRLS_BIND(brls::Label, button, "gallery/label");
};

HintActivity::HintActivity() {
    brls::Logger::debug("HintActivityActivity: create");
}

void HintActivity::onContentAvailable() {
    brls::Logger::debug("HintActivityActivity: onContentAvailable");

#ifdef BUILTIN_NSP
    gallery->setData({
        {"pictures/hint_game_1.png", "wiliwili/hint1"_i18n},
        {"pictures/hint_game_2.png", "wiliwili/hint2"_i18n},
        {"pictures/hint_hbmenu.png", "wiliwili/hint3"_i18n},
    });
    gallery->addCustomView(new GalleryItemInstallNSP());
#else
    gallery->setData({
        {"pictures/hint_game_1.png", "wiliwili/hint1"_i18n},
        {"pictures/hint_game_2.png", "wiliwili/hint2"_i18n},
        {"pictures/hint_hbmenu.png", "wiliwili/hint3"_i18n},
        {"pictures/hint_wiliwili.png", "wiliwili/hint4"_i18n},
    });
#endif
}

HintActivity::~HintActivity() {
    brls::Logger::debug("HintActivityActivity: delete");
}