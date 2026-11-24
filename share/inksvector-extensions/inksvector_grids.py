"""Grades, guias e alinhamento para a galeria do Inksvector."""
# SPDX-License-Identifier: GPL-2.0-or-later
import math

import inkex
from lxml import etree


class Grids(inkex.EffectExtension):
    def add_arguments(self, pars):
        pars.add_argument("--mode", required=True)
        pars.add_argument("--divisions", type=int, default=8)
        pars.add_argument("--spacing", type=float, default=10)
        pars.add_argument("--margin", type=float, default=10)

    def effect(self):
        mode = self.options.mode
        count = max(2, min(self.options.divisions, 64))
        if mode in {"align-grid", "align", "snap-guides"}:
            self.align_selection(mode)
            return

        page = self.svg.get_page_bbox()
        left, top, width, height = page.left, page.top, page.width, page.height
        if width <= 0 or height <= 0:
            raise inkex.AbortExtension("A página deve ter largura e altura positivas.")

        def point(x, y):
            return left + x * width, top + y * height

        center = point(0.5, 0.5)
        xmarks = [left + width * i / count for i in range(1, count)]
        ymarks = [top + height * i / count for i in range(1, count)]
        view = self.svg.namedview

        if mode in {"square", "isometric", "grid-guides"}:
            spacing = self.svg.viewport_to_unit(f"{max(self.options.spacing, 0.1)}mm")
            # Unitless grid attributes are expressed in document user units.
            attrs = dict(type="axonomgrid" if mode == "isometric" else "xygrid",
                         originx=str(left), originy=str(top), spacingy=str(spacing), units="mm")
            if mode == "isometric":
                attrs.update(gridanglex="30", gridanglez="30")
            else:
                attrs["spacingx"] = str(spacing)
            etree.SubElement(view, inkex.addNS("grid", "inkscape"), **attrs)
            view.set("showgrid", "true")
            if mode != "grid-guides":
                return

        def guide(position, direction=True):
            view.add_guide(position, direction)

        def axes(xs, ys):
            for x in xs:
                guide(x, False)
            for y in ys:
                guide(y)

        if mode in {"cross", "grid-guides"}:
            axes([center[0]], [center[1]])
        elif mode == "cross45":
            for angle in (-45, 45):
                guide(center, angle)
        elif mode in {"multi-guides", "subdivisions"}:
            axes(xmarks, ymarks)
            if mode == "multi-guides":
                for angle in (-45, 45):
                    guide(center, angle)
        elif mode == "diagonal":
            for x in xmarks:
                for angle in (-45, 45):
                    guide((x, top + height / 2), angle)
        elif mode == "multi-cross":
            axes([point(i, 0)[0] for i in (0.25, 0.5, 0.75)],
                 [point(0, i)[1] for i in (0.25, 0.5, 0.75)])
        elif mode == "radial":
            for i in range(count):
                guide(center, i * 180 / count)
        elif mode == "margin":
            fraction = max(1, min(49, self.options.margin)) / 100
            axes([point(i, 0)[0] for i in (fraction, 1 - fraction)],
                 [point(0, i)[1] for i in (fraction, 1 - fraction)])
        elif mode in {"perspective", "perspective-grid", "curves", "guided-geometry"}:
            self.draw_geometry(mode, count, point, width)
        else:
            raise inkex.AbortExtension("Modo de grade desconhecido.")

        if mode not in {"perspective", "perspective-grid", "curves", "guided-geometry"}:
            view.set("showguides", "true")

    def draw_geometry(self, mode, count, point, width):
        commands = []

        def coord(x, y):
            return ",".join(str(value) for value in point(x, y))

        def line(a, b):
            commands.append(f"M{coord(*a)} L{coord(*b)}")

        if mode == "curves":
            for i in range(1, count):
                t = i / count
                commands.append(f"M{coord(0, t)} Q{coord(t, t)} {coord(t, 1)}")
                commands.append(f"M{coord(t, 0)} Q{coord(t, t)} {coord(1, t)}")
        elif mode == "guided-geometry":
            points = [(0.5 + 0.4 * math.cos(i * math.tau / count),
                       0.5 + 0.4 * math.sin(i * math.tau / count)) for i in range(count)]
            commands.append("M" + " L".join(coord(*p) for p in points) + " Z")
            for p in points:
                line((0.5, 0.5), p)
        else:
            vanishing_points = [(0.5, 0.3)] if mode == "perspective" else [(0, 0.3), (1, 0.3)]
            for i in range(count + 1):
                t = i / count
                for vp in vanishing_points:
                    line(vp, (t, 1))
                depth = 0.3 + 0.7 * t * t
                left = (1 - t * t) / 2 if len(vanishing_points) == 1 else 0
                line((left, depth), (1 - left, depth))
        layer = inkex.Layer.new(f"Grades e Linhas — {mode}")
        self.svg.add(layer)
        path = layer.add(inkex.PathElement())
        path.set("d", " ".join(commands))
        path.style = {"fill": "none", "stroke": "#286aaa", "stroke-width": width / 600}

    def align_selection(self, mode):
        objects = list(self.svg.selection.values())
        selected = set(objects)
        centers = []
        for obj in objects:
            if not isinstance(obj, inkex.ShapeElement):
                continue
            if any(parent in selected for parent in obj.iterancestors()):
                continue
            box = obj.bounding_box(True)
            if box:
                centers.append((obj, box.center))
        if not centers:
            raise inkex.AbortExtension("Selecione pelo menos um objeto com geometria.")

        spacing = self.svg.viewport_to_unit(f"{max(self.options.spacing, 0.1)}mm")
        page = self.svg.get_page_bbox()
        origin = inkex.Vector2d(page.left, page.top)
        guides = self.svg.namedview.get_guides() if mode == "snap-guides" else []
        if mode == "snap-guides" and not guides:
            raise inkex.AbortExtension("Crie pelo menos uma guia antes de encaixar a seleção.")

        anchor = centers[0][1]
        for obj, center in centers:
            if mode == "align-grid":
                target = origin + inkex.Vector2d(*(round(value / spacing) * spacing for value in center - origin))
            elif mode == "align":
                target = inkex.Vector2d(anchor.x, center.y)
            else:
                offsets = []
                for guide in guides:
                    normal = inkex.Vector2d(guide.orientation.x, -guide.orientation.y)
                    length = math.hypot(*normal)
                    if length:
                        nx, ny = normal.x / length, normal.y / length
                        delta = guide.position - center
                        distance = nx * delta.x + ny * delta.y
                        offsets.append((distance * nx, distance * ny))
                if not offsets:
                    continue
                target = center + inkex.Vector2d(min(offsets, key=lambda offset: math.hypot(*offset)))
            delta = target - center
            parent = obj.getparent().composed_transform()
            try:
                obj.transform = (-parent) @ inkex.Transform(translate=(delta.x, delta.y)) @ parent @ obj.transform
            except ZeroDivisionError:
                raise inkex.AbortExtension("Um grupo tem transformação sem inversa.") from None


if __name__ == "__main__":
    Grids().run()
