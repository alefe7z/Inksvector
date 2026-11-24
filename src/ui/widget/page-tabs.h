// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/viewport.h>
#include <sigc++/scoped_connection.h>
#include <vector>

class SPDesktop;
class SPDocument;

namespace Inkscape::UI::Widget {

class PageTabs : public Gtk::Box
{
public:
    PageTabs();
    void setDesktop(SPDesktop *desktop);

private:
    void bind(SPDocument *document);
    void rebuild();
    void update();
    void select(int index);
    void insert(bool before);

    SPDesktop *_desktop = nullptr;
    SPDocument *_document = nullptr;
    Gtk::Button _before, _first, _prev, _next, _last, _after;
    Gtk::Label _count;
    Gtk::ScrolledWindow _scroll;
    Gtk::Viewport _viewport;
    Gtk::Box _tabs;
    std::vector<Gtk::Button *> _buttons;
    std::vector<sigc::scoped_connection> _labels;
    sigc::scoped_connection _replacement, _pages, _selection, _idle;
};
}
