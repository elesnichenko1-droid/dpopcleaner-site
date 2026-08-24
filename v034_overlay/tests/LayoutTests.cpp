#include "ui/Layout.h"
#include <iostream>
#include <utility>

namespace {
bool Check(bool c, const char* m) { if (!c) { std::cerr << "LayoutTests FAILED: " << m << '\n'; return false; } return true; }
bool Inside(const dpop::ui::Box& c, const dpop::ui::Box& p) {
    return c.x >= p.x && c.y >= p.y && c.x + c.width <= p.x + p.width && c.y + c.height <= p.y + p.height;
}
bool Disjoint(const dpop::ui::Box& a, const dpop::ui::Box& b) {
    return a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y;
}
}

int main() {
    using namespace dpop::ui;
    const std::pair<int,int> sizes[]={{1100,700},{1200,850},{1920,1080}};
    for (const auto [w,h]: sizes) {
        const auto l=ComputeShellLayout(w,h);
        if (!Check(l.sidebar.x==0 && l.sidebar.y==0 && l.sidebar.height==h,"sidebar must fill left edge")) return 1;
        if (!Check(l.sidebar.width>=200 && l.sidebar.width<=240,"sidebar width must remain compact")) return 2;
        if (!Check(l.content.x>=l.sidebar.width,"content must start right of sidebar")) return 3;
        if (!Check(Disjoint(l.sidebar,l.content),"sidebar and content must not overlap")) return 4;
        if (!Check(l.footer.x==l.sidebar.width && l.footer.y+l.footer.height==h,"footer must occupy only right pane bottom")) return 5;
        if (!Check(l.content.y+l.content.height<l.footer.y,"content must not overlap footer")) return 6;
        if (!Check(Inside(l.navigation,l.sidebar),"navigation must stay inside sidebar")) return 7;
        for (std::size_t i=0;i<l.navButtons.size();++i) {
            if (!Check(Inside(l.navButtons[i],l.sidebar),"nav button must stay inside sidebar")) return 8;
            if (i && !Check(l.navButtons[i].y>l.navButtons[i-1].y,"nav rows must be strictly vertical")) return 9;
        }
        if (!Check(l.navButtons.back().y+l.navButtons.back().height <= h-90,"all 13 sections must fit minimum height")) return 10;
        if (!Check(l.content.width>=840,"content must stay useful at minimum width")) return 11;
        if (!Check(l.content.height>=520,"content must stay useful at minimum height")) return 12;
        if (!Check(Inside(l.status,l.footer)&&Inside(l.log,l.footer)&&Inside(l.support,l.footer)&&Inside(l.version,l.footer),"status controls must stay in footer")) return 13;
    }
    const auto clamped=ComputeShellLayout(800,500);
    if (!Check(clamped.sidebar.height==700,"height below minimum must clamp")) return 14;
    if (!Check(clamped.footer.x+clamped.footer.width==1100,"width below minimum must clamp")) return 15;
    std::cout<<"LayoutTests PASS\n";
    return 0;
}
