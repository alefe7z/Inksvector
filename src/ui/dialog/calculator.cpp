// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Calculator dialog backed by libqalculate (Extensions > MATH).
 */
/* Copyright (C) 2026 Inksvector contributors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "calculator.h"

#include <algorithm>
#include <clocale>
#include <iterator>
#include <optional>
#include <thread>

#include <glib.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <gdkmm/clipboard.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/listboxrow.h>
#include <gtkmm/stringlist.h>

#include "preferences.h"

// libqalculate declares many names in the global namespace (Unit, Number, Variable, ...) and
// includes <windows.h> on Windows. Keep it last so that it cannot interfere with the Inkscape and
// GTK headers above; std::min/std::max are parenthesized below in case min/max macros leak in.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <libqalculate/qalculate.h>

namespace Inkscape::UI::Dialog {

namespace {

constexpr auto PREFS_PATH = "/dialogs/calculator/";
constexpr int CALCULATION_TIMEOUT_MS = 5000;
constexpr std::size_t MAX_HISTORY = 200;

/// Base values matching the entries of the "Result base" drop-down.
constexpr int RESULT_BASES[] = {BASE_DECIMAL, BASE_HEXADECIMAL, BASE_OCTAL, BASE_BINARY};
/// Angle units matching the entries of the "Angle" drop-down.
constexpr AngleUnit ANGLE_UNITS[] = {ANGLE_UNIT_RADIANS, ANGLE_UNIT_DEGREES, ANGLE_UNIT_GRADIANS};

KnownVariable *answer_variable = nullptr;
bool definitions_loaded = false;

/**
 * Create the global libqalculate engine once per process.
 *
 * The Calculator constructor calls setlocale(LC_ALL, "") and, on Windows, may set the LANGUAGE
 * environment variable. Inkscape relies on its own locale setup, so both are restored after
 * construction.
 */
void ensure_engine()
{
    if (CALCULATOR) {
        return;
    }

    char const *current_locale = std::setlocale(LC_ALL, nullptr);
    std::string const saved_locale = current_locale ? current_locale : "C";
    char const *current_language = g_getenv("LANGUAGE");
    std::optional<std::string> const saved_language =
        current_language ? std::optional<std::string>(current_language) : std::nullopt;

    new ::Calculator();

    std::setlocale(LC_ALL, saved_locale.c_str());
    if (saved_language) {
        g_setenv("LANGUAGE", saved_language->c_str(), TRUE);
    } else {
        g_unsetenv("LANGUAGE");
    }

    definitions_loaded = CALCULATOR->loadGlobalDefinitions();
    CALCULATOR->loadLocalDefinitions();
    CALCULATOR->loadExchangeRates();
    CALCULATOR->setPrecision(Preferences::get()->getIntLimited(Glib::ustring(PREFS_PATH) + "precision",
                                                               DEFAULT_PRECISION, 2, 1000));

    if (auto existing = dynamic_cast<KnownVariable *>(CALCULATOR->getActiveVariable("ans"))) {
        answer_variable = existing;
    } else {
        answer_variable = new KnownVariable("Temporary", "ans", MathStructure(), "Last Answer", false);
        CALCULATOR->addVariable(answer_variable);
    }
}

Gtk::Label *make_label(Glib::ustring const &text, Gtk::Align halign)
{
    auto label = Gtk::make_managed<Gtk::Label>(text);
    label->set_halign(halign);
    label->set_xalign(halign == Gtk::Align::END ? 1.0f : 0.0f);
    label->set_wrap(true);
    label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    return label;
}

struct RatesResult
{
    std::weak_ptr<bool> alive;
    CalculatorDialog *dialog;
    bool success;
};

} // namespace

CalculatorDialog::CalculatorDialog()
    : DialogBase("/dialogs/calculator", "Calculator")
    , _main(Gtk::Orientation::VERTICAL, 6)
    , _options(Gtk::Orientation::VERTICAL, 4)
    , _angle_dropdown(std::vector<Glib::ustring>{_("Radians"), _("Degrees"), _("Gradians")})
    , _base_dropdown(std::vector<Glib::ustring>{_("Decimal"), _("Hexadecimal"), _("Octal"), _("Binary")})
    , _exact_check(_("Exact results"))
    , _decimal_comma_check(_("Decimal comma"))
    , _copy_button(_("Copy result"))
    , _clear_history_button(_("Clear history"))
    , _rates_button(_("Update exchange rates"))
    , _alive(std::make_shared<bool>(true))
{
    ensure_engine();

    _main.set_margin(8);
    _main.set_vexpand(true);
    append(_main);

    // History (newest calculation on top).
    _history_list.set_selection_mode(Gtk::SelectionMode::NONE);
    _history_list.set_activate_on_single_click(true);
    _history_list.set_tooltip_text(_("Click a calculation to edit it again"));
    _history_list.signal_row_activated().connect([this](Gtk::ListBoxRow *row) {
        auto const index = static_cast<int>(_history.size()) - 1 - row->get_index();
        if (index >= 0 && index < static_cast<int>(_history.size())) {
            _entry.set_text(_history[index].expression);
            _entry.grab_focus();
            _entry.set_position(-1);
        }
    });
    _history_scroll.set_child(_history_list);
    _history_scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    _history_scroll.set_min_content_height(120);
    _history_scroll.set_vexpand(true);
    _history_scroll.set_has_frame(true);
    _main.append(_history_scroll);

    // Result area.
    _parsed_label.set_halign(Gtk::Align::END);
    _parsed_label.set_xalign(1.0f);
    _parsed_label.set_wrap(true);
    _parsed_label.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    _parsed_label.set_selectable(true);
    _parsed_label.add_css_class("dim-label");
    _main.append(_parsed_label);

    _result_label.set_halign(Gtk::Align::END);
    _result_label.set_xalign(1.0f);
    _result_label.set_wrap(true);
    _result_label.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    _result_label.set_selectable(true);
    _result_label.set_markup("<span size='x-large' weight='bold'>0</span>");
    _main.append(_result_label);

    _message_label.set_halign(Gtk::Align::START);
    _message_label.set_xalign(0.0f);
    _message_label.set_wrap(true);
    _message_label.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    _message_label.set_visible(false);
    _main.append(_message_label);

    // Expression entry.
    _entry.set_hexpand(true);
    _entry.set_placeholder_text(_("Expression, e.g. 2^10, sqrt(2), 5 ft to mm, 255 to hex"));
    _entry.signal_activate().connect(sigc::mem_fun(*this, &CalculatorDialog::calculate));
    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) {
            if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up) {
                recall_history(-1);
                return true;
            }
            if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down) {
                recall_history(1);
                return true;
            }
            return false;
        },
        false);
    _entry.add_controller(keys);
    _main.append(_entry);

    build_keypad();
    build_options();

    if (!definitions_loaded) {
        show_message(_("The Qalculate definitions (units, functions and constants) were not found. "
                       "Only basic arithmetic is available."),
                     true);
    }
}

CalculatorDialog::~CalculatorDialog()
{
    *_alive = false;
}

void CalculatorDialog::focus_dialog()
{
    DialogBase::focus_dialog();
    _entry.grab_focus();
}

void CalculatorDialog::build_keypad()
{
    struct Key
    {
        char const *label;
        char const *text;
        char const *tooltip;
    };
    // An empty text marks a key with a special action, identified by its label.
    static Key const keys[][6] = {
        {{"sin", "sin(", N_("Sine")},
         {"cos", "cos(", N_("Cosine")},
         {"tan", "tan(", N_("Tangent")},
         {"ln", "ln(", N_("Natural logarithm")},
         {"log₁₀", "log10(", N_("Base-10 logarithm")},
         {"√", "sqrt(", N_("Square root")}},
        {{"sin⁻¹", "asin(", N_("Arcsine")},
         {"cos⁻¹", "acos(", N_("Arccosine")},
         {"tan⁻¹", "atan(", N_("Arctangent")},
         {"eˣ", "exp(", N_("Exponential")},
         {"xʸ", "^", N_("Power")},
         {"x²", "^2", N_("Square")}},
        {{"7", "7", nullptr},
         {"8", "8", nullptr},
         {"9", "9", nullptr},
         {"÷", "/", N_("Divide")},
         {"(", "(", nullptr},
         {")", ")", nullptr}},
        {{"4", "4", nullptr},
         {"5", "5", nullptr},
         {"6", "6", nullptr},
         {"×", "*", N_("Multiply")},
         {"π", "pi", N_("Pi")},
         {"e", "e", N_("Euler's number")}},
        {{"1", "1", nullptr},
         {"2", "2", nullptr},
         {"3", "3", nullptr},
         {"−", "-", N_("Subtract")},
         {"!", "!", N_("Factorial")},
         {"%", "%", N_("Percent")}},
        {{"0", "0", nullptr},
         {"DOT", "", N_("Decimal separator")},
         {"E", "E", N_("Times ten to the power of")},
         {"+", "+", N_("Add")},
         {"Ans", "ans", N_("Last answer")},
         {"=", "", N_("Calculate (Enter)")}},
        {{"C", "", N_("Clear the expression")},
         {"⌫", "", N_("Delete the previous character")},
         {"to", " to ", N_("Convert to a unit or base, e.g. 5 ft to mm")},
         {"COMMA", "", N_("Function argument separator")},
         {"x", "x", N_("Variable x")},
         {"mod", " mod ", N_("Remainder")}},
    };

    _keypad.set_row_spacing(4);
    _keypad.set_column_spacing(4);
    _keypad.set_row_homogeneous(true);
    _keypad.set_column_homogeneous(true);

    for (int row = 0; row < static_cast<int>(std::size(keys)); ++row) {
        for (int column = 0; column < 6; ++column) {
            auto const &key = keys[row][column];
            Glib::ustring const label = key.label;
            auto button = Gtk::make_managed<Gtk::Button>();
            if (key.tooltip) {
                button->set_tooltip_text(_(key.tooltip));
            }
            button->set_focus_on_click(false);
            button->set_hexpand(true);

            if (label == "DOT") {
                button->set_label(CALCULATOR->getDecimalPoint());
                button->signal_clicked().connect([this] { insert_text(CALCULATOR->getDecimalPoint()); });
                _decimal_button = button;
            } else if (label == "COMMA") {
                button->set_label(CALCULATOR->getComma());
                button->signal_clicked().connect([this] { insert_text(CALCULATOR->getComma() + " "); });
                _comma_button = button;
            } else {
                button->set_label(label);
                if (label == "=") {
                    button->add_css_class("suggested-action");
                    button->signal_clicked().connect(sigc::mem_fun(*this, &CalculatorDialog::calculate));
                } else if (label == "C") {
                    button->signal_clicked().connect(sigc::mem_fun(*this, &CalculatorDialog::clear_entry));
                } else if (label == "⌫") {
                    button->signal_clicked().connect(sigc::mem_fun(*this, &CalculatorDialog::backspace));
                } else {
                    Glib::ustring const text = key.text;
                    button->signal_clicked().connect([this, text] { insert_text(text); });
                }
            }
            _keypad.attach(*button, column, row);
        }
    }
    _main.append(_keypad);
}

void CalculatorDialog::build_options()
{
    auto prefs = Preferences::get();
    Glib::ustring const path = PREFS_PATH;

    // Options and actions flow onto several lines when the dialog is docked in a narrow panel.
    auto flow = Gtk::make_managed<Gtk::FlowBox>();
    flow->set_selection_mode(Gtk::SelectionMode::NONE);
    flow->set_homogeneous(false);
    flow->set_column_spacing(6);
    flow->set_row_spacing(4);
    flow->set_max_children_per_line(8);
    auto add = [flow](Gtk::Widget &widget) {
        flow->append(widget);
        if (auto child = widget.get_parent()) {
            child->set_focusable(false);
        }
    };
    auto labelled = [&add](Glib::ustring const &caption, Gtk::Widget &control) {
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
        box->append(*Gtk::make_managed<Gtk::Label>(caption));
        box->append(control);
        add(*box);
    };

    _angle_dropdown.set_selected(prefs->getIntLimited(path + "angle", 0, 0, 2));
    _angle_dropdown.set_tooltip_text(_("Angle unit used by trigonometric functions"));
    labelled(_("Angle:"), _angle_dropdown);

    _base_dropdown.set_selected(prefs->getIntLimited(path + "base", 0, 0, 3));
    _base_dropdown.set_tooltip_text(_("Number base of the result"));
    labelled(_("Result:"), _base_dropdown);

    _precision_spin.set_range(2, 1000);
    _precision_spin.set_increments(1, 10);
    _precision_spin.set_digits(0);
    _precision_spin.set_width_chars(4);
    _precision_spin.set_value(prefs->getIntLimited(path + "precision", DEFAULT_PRECISION, 2, 1000));
    _precision_spin.set_tooltip_text(_("Number of significant digits"));
    labelled(_("Precision:"), _precision_spin);

    _exact_check.set_active(prefs->getBool(path + "exact", false));
    _exact_check.set_tooltip_text(_("Keep fractions, roots and constants exact instead of approximating them"));
    add(_exact_check);

    _decimal_comma_check.set_active(prefs->getBool(path + "decimal-comma", CALCULATOR->getDecimalPoint() == ","));
    _decimal_comma_check.set_tooltip_text(
        _("Use a comma as decimal separator (1,5). Function arguments are then separated by semicolons."));
    add(_decimal_comma_check);

    _copy_button.set_tooltip_text(_("Copy the result to the clipboard"));
    _copy_button.signal_clicked().connect(sigc::mem_fun(*this, &CalculatorDialog::copy_result));
    add(_copy_button);

    _clear_history_button.signal_clicked().connect([this] {
        _history.clear();
        _history_cursor = -1;
        while (auto row = _history_list.get_row_at_index(0)) {
            _history_list.remove(*row);
        }
    });
    add(_clear_history_button);

    _rates_button.set_tooltip_text(_("Download current currency exchange rates from the Internet"));
    _rates_button.set_sensitive(CALCULATOR->canFetch());
    _rates_button.signal_clicked().connect(sigc::mem_fun(*this, &CalculatorDialog::update_exchange_rates));
    add(_rates_button);

    _options.append(*flow);

    auto on_change = [this] { apply_options(); };
    _angle_dropdown.property_selected().signal_changed().connect(on_change);
    _base_dropdown.property_selected().signal_changed().connect(on_change);
    _exact_check.signal_toggled().connect(on_change);
    _decimal_comma_check.signal_toggled().connect(on_change);
    _precision_spin.signal_value_changed().connect(on_change);

    _main.append(_options);
    apply_options();
}

void CalculatorDialog::apply_options()
{
    auto prefs = Preferences::get();
    Glib::ustring const path = PREFS_PATH;
    prefs->setInt(path + "angle", _angle_dropdown.get_selected());
    prefs->setInt(path + "base", _base_dropdown.get_selected());
    prefs->setBool(path + "exact", _exact_check.get_active());
    prefs->setBool(path + "decimal-comma", _decimal_comma_check.get_active());
    prefs->setInt(path + "precision", _precision_spin.get_value_as_int());

    if (_decimal_comma_check.get_active()) {
        CALCULATOR->useDecimalComma();
    } else {
        CALCULATOR->useDecimalPoint(false);
    }
    CALCULATOR->setPrecision(_precision_spin.get_value_as_int());

    if (_decimal_button) {
        _decimal_button->set_label(CALCULATOR->getDecimalPoint());
    }
    if (_comma_button) {
        _comma_button->set_label(CALCULATOR->getComma());
    }
}

void CalculatorDialog::insert_text(Glib::ustring const &text)
{
    int start = 0;
    int end = 0;
    bool const function = !text.empty() && text[text.length() - 1] == '(';
    if (_entry.get_selection_bounds(start, end) && start != end) {
        // Wrap the selection when a function is inserted, replace it otherwise.
        Glib::ustring const selected = _entry.get_chars(start, end);
        _entry.delete_text(start, end);
        Glib::ustring const replacement = function ? text + selected + ")" : text;
        int position = start;
        _entry.insert_text(replacement, -1, position);
        _entry.set_position(position);
    } else {
        int position = _entry.get_position();
        _entry.insert_text(text, -1, position);
        _entry.set_position(position);
    }
    _history_cursor = -1;
    _entry.grab_focus_without_selecting();
}

void CalculatorDialog::backspace()
{
    int start = 0;
    int end = 0;
    if (_entry.get_selection_bounds(start, end) && start != end) {
        _entry.delete_text(start, end);
    } else if (int const position = _entry.get_position(); position > 0) {
        _entry.delete_text(position - 1, position);
    }
    _entry.grab_focus_without_selecting();
}

void CalculatorDialog::clear_entry()
{
    _entry.set_text("");
    _parsed_label.set_text("");
    show_message("", false);
    _history_cursor = -1;
    _entry.grab_focus_without_selecting();
}

void CalculatorDialog::calculate()
{
    Glib::ustring const expression = _entry.get_text();
    if (expression.find_first_not_of(" \t") == Glib::ustring::npos) {
        return;
    }

    EvaluationOptions eo = default_user_evaluation_options;
    eo.parse_options.angle_unit = ANGLE_UNITS[(std::min<guint>)(_angle_dropdown.get_selected(), 2)];
    eo.approximation = _exact_check.get_active() ? APPROXIMATION_EXACT : APPROXIMATION_TRY_EXACT;

    PrintOptions po = default_print_options;
    po.base = RESULT_BASES[(std::min<guint>)(_base_dropdown.get_selected(), 3)];
    po.use_unicode_signs = true;
    po.division_sign = DIVISION_SIGN_SLASH;
    po.interval_display = INTERVAL_DISPLAY_SIGNIFICANT_DIGITS;
    po.number_fraction_format = _exact_check.get_active() ? FRACTION_FRACTIONAL : FRACTION_DECIMAL;
    bool approximate = false;
    po.is_approximate = &approximate;

    // "ans" refers to the previous result.
    if (answer_variable && !_last_result.empty()) {
        answer_variable->set(CALCULATOR->unlocalizeExpression(_last_result.raw(), eo.parse_options));
    }

    CALCULATOR->clearMessages();
    std::string const input = CALCULATOR->unlocalizeExpression(expression.raw(), eo.parse_options);
#if QALCULATE_MAJOR_VERSION >= 5
    std::string parsed;
    std::string const result = CALCULATOR->calculateAndPrint(input, CALCULATION_TIMEOUT_MS, eo, po, &parsed);
#else
    std::string const parsed;
    std::string const result = CALCULATOR->calculateAndPrint(input, CALCULATION_TIMEOUT_MS, eo, po);
#endif

    // Collect errors and warnings produced by the calculation.
    Glib::ustring messages;
    bool has_error = false;
    for (auto message = CALCULATOR->message(); message; message = CALCULATOR->nextMessage()) {
        if (message->type() == MESSAGE_ERROR) {
            has_error = true;
        }
        if (!messages.empty()) {
            messages += "\n";
        }
        messages += message->message();
    }
    show_message(messages, has_error);

    if (result.empty()) {
        return;
    }

    _parsed_label.set_text(parsed.empty() ? expression : Glib::ustring(parsed));
    Glib::ustring const sign = approximate ? "≈ " : "= ";
    _result_label.set_markup("<span size='x-large' weight='bold'>" + Glib::Markup::escape_text(sign + result) +
                             "</span>");
    _last_result = result;
    add_history(expression, (approximate ? "≈ " : "= ") + Glib::ustring(result));
    _history_cursor = -1;
}

void CalculatorDialog::add_history(Glib::ustring const &expression, Glib::ustring const &result)
{
    if (_history.size() >= MAX_HISTORY) {
        _history.erase(_history.begin());
        if (auto last = _history_list.get_row_at_index(static_cast<int>(MAX_HISTORY) - 1)) {
            _history_list.remove(*last);
        }
    }
    _history.push_back({expression, result});

    auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    box->set_margin(4);
    auto expression_label = make_label(expression, Gtk::Align::START);
    expression_label->add_css_class("dim-label");
    box->append(*expression_label);
    auto result_label = make_label(result, Gtk::Align::END);
    result_label->set_markup("<b>" + Glib::Markup::escape_text(result) + "</b>");
    box->append(*result_label);
    _history_list.prepend(*box);
}

void CalculatorDialog::recall_history(int direction)
{
    if (_history.empty()) {
        return;
    }
    int const size = static_cast<int>(_history.size());
    if (_history_cursor < 0) {
        if (direction > 0) {
            return;
        }
        _history_cursor = size - 1;
    } else {
        _history_cursor += direction;
    }
    if (_history_cursor >= size) {
        _history_cursor = -1;
        _entry.set_text("");
        return;
    }
    _history_cursor = (std::max)(_history_cursor, 0);
    _entry.set_text(_history[_history_cursor].expression);
    _entry.set_position(-1);
}

void CalculatorDialog::copy_result()
{
    if (!_last_result.empty()) {
        get_clipboard()->set_text(_last_result);
    }
}

void CalculatorDialog::update_exchange_rates()
{
    _rates_button.set_sensitive(false);
    show_message(_("Downloading exchange rates..."), false);

    auto payload = new RatesResult{_alive, this, false};
    std::thread([payload] {
        payload->success = CALCULATOR->fetchExchangeRates(15);
        // Back to the GTK main loop; g_idle_add is safe to call from any thread.
        g_idle_add(
            [](gpointer data) -> gboolean {
                std::unique_ptr<RatesResult> result(static_cast<RatesResult *>(data));
                if (result->success) {
                    CALCULATOR->loadExchangeRates();
                }
                CALCULATOR->clearMessages();
                if (auto alive = result->alive.lock(); alive && *alive) {
                    result->dialog->_rates_button.set_sensitive(true);
                    result->dialog->show_message(result->success ? _("Exchange rates updated.")
                                                                 : _("Could not download exchange rates."),
                                                 !result->success);
                }
                return G_SOURCE_REMOVE;
            },
            payload);
    }).detach();
}

void CalculatorDialog::show_message(Glib::ustring const &text, bool is_error)
{
    _message_label.set_text(text);
    _message_label.set_visible(!text.empty());
    if (is_error) {
        _message_label.add_css_class("error");
    } else {
        _message_label.remove_css_class("error");
    }
}

} // namespace Inkscape::UI::Dialog

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
