// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_TOOLS_VIRTUAL_SEGMENTS_H
#define INKSCAPE_UI_TOOLS_VIRTUAL_SEGMENTS_H

#include <algorithm>
#include <cmath>
#include <vector>

#include <2geom/pathvector.h>

namespace Inkscape::UI::Tools {

using VirtualCuts = std::vector<std::vector<double>>;

inline void collectVirtualCuts(VirtualCuts &cuts, Geom::PathVector const &path,
                               Geom::PathVector const &other, bool same_object)
{
    for (auto const &hit : same_object ? path.intersectSelf() : path.intersect(other)) {
        cuts[hit.first.path_index].push_back(hit.first.asFlatTime());
        if (same_object) {
            cuts[hit.second.path_index].push_back(hit.second.asFlatTime());
        }
    }
}

/**
 * Remove selected intervals of an outline without flattening curves. For a closed
 * outline the intervals at either side of its initial node are one segment.
 */
template <typename ShouldRemove>
bool removeVirtualSegments(Geom::PathVector const &source, VirtualCuts cuts,
                           ShouldRemove should_remove, Geom::PathVector &result)
{
    constexpr double tolerance = 1e-7;
    bool changed = false;
    for (std::size_t index = 0; index < source.size(); ++index) {
        auto const &path = source[index];
        if (path.empty()) {
            result.push_back(path);
            continue;
        }

        double const end = path.size();
        auto &boundaries = cuts[index];
        std::erase_if(boundaries, [end](double value) {
            return !std::isfinite(value) || value < -tolerance || value > end + tolerance;
        });
        for (auto &value : boundaries) {
            if (value < tolerance || (path.closed() && value > end - tolerance)) {
                value = 0;
            } else if (value > end - tolerance) {
                value = end;
            }
        }
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end(), [](double a, double b) {
            return std::abs(a - b) < tolerance;
        }), boundaries.end());

        if (!path.closed() || boundaries.empty()) {
            if (boundaries.empty() || boundaries.front() != 0) {
                boundaries.insert(boundaries.begin(), 0);
            }
            if (!path.closed() && boundaries.back() != end) {
                boundaries.push_back(end);
            }
        }

        auto portion = [&](double from, double to) {
            auto segment = path.portion(from, std::min(to, end));
            if (to > end) {
                segment.append(path.portion(0, to - end));
            }
            return segment;
        };

        auto const count = path.closed() ? boundaries.size() : boundaries.size() - 1;
        std::vector<std::pair<double, double>> removed;
        for (std::size_t part = 0; part < count; ++part) {
            double const from = boundaries[part];
            double const to = part + 1 < boundaries.size() ? boundaries[part + 1] : boundaries.front() + end;
            if (!should_remove(index, from, to, end, portion(from, to))) {
                continue;
            }
            removed.emplace_back(from, std::min(to, end));
            if (to > end + tolerance) {
                removed.emplace_back(0, to - end);
            }
        }

        if (removed.empty()) {
            result.push_back(path);
            continue;
        }

        changed = true;
        std::sort(removed.begin(), removed.end());
        double cursor = 0;
        std::vector<std::pair<double, double>> kept;
        for (auto const &[from, to] : removed) {
            if (from > cursor + tolerance) {
                kept.emplace_back(cursor, from);
            }
            cursor = std::max(cursor, to);
        }
        if (cursor < end - tolerance) {
            kept.emplace_back(cursor, end);
        }
        if (path.closed() && kept.size() > 1 && kept.front().first == 0 && kept.back().second == end) {
            result.push_back(portion(kept.back().first, end + kept.front().second));
            kept.pop_back();
            kept.erase(kept.begin());
        }
        for (auto const &[from, to] : kept) {
            result.push_back(portion(from, to));
        }
    }
    return changed;
}

} // namespace Inkscape::UI::Tools

#endif
