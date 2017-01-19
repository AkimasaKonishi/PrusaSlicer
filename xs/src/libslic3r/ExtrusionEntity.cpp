#include "ExtrusionEntity.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "ExPolygonCollection.hpp"
#include "ClipperUtils.hpp"
#include "Extruder.hpp"
#include "Flow.hpp"
#include <cmath>
#include <limits>
#include <sstream>

namespace Slic3r {
    
void
ExtrusionPath::intersect_expolygons(const ExPolygonCollection &collection, ExtrusionEntityCollection* retval) const
{
    this->_inflate_collection(intersection_pl(this->polyline, collection), retval);
}

void
ExtrusionPath::subtract_expolygons(const ExPolygonCollection &collection, ExtrusionEntityCollection* retval) const
{
    this->_inflate_collection(diff_pl(this->polyline, collection), retval);
}

void
ExtrusionPath::clip_end(double distance)
{
    this->polyline.clip_end(distance);
}

void
ExtrusionPath::simplify(double tolerance)
{
    this->polyline.simplify(tolerance);
}

double
ExtrusionPath::length() const
{
    return this->polyline.length();
}

void
ExtrusionPath::_inflate_collection(const Polylines &polylines, ExtrusionEntityCollection* collection) const
{
    for (Polylines::const_iterator it = polylines.begin(); it != polylines.end(); ++it) {
        ExtrusionPath* path = this->clone();
        path->polyline = *it;
        collection->entities.push_back(path);
    }
}

void ExtrusionPath::polygons_covered_by_width(Polygons &out, const float scaled_epsilon) const
{
    polygons_append(out, offset(this->polyline, float(scale_(this->width/2)) + scaled_epsilon));
}

void ExtrusionPath::polygons_covered_by_spacing(Polygons &out, const float scaled_epsilon) const
{
    // Instantiating the Flow class to get the line spacing.
    // Don't know the nozzle diameter, setting to zero. It shall not matter it shall be optimized out by the compiler.
    Flow flow(this->width, this->height, 0.f, this->is_bridge());
    polygons_append(out, offset(this->polyline, 0.5f * float(flow.scaled_spacing()) + scaled_epsilon));
}

void ExtrusionMultiPath::reverse()
{
    for (ExtrusionPaths::iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->reverse();
    std::reverse(this->paths.begin(), this->paths.end());
}

double ExtrusionMultiPath::length() const
{
    double len = 0;
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        len += path->polyline.length();
    return len;
}

void ExtrusionMultiPath::polygons_covered_by_width(Polygons &out, const float scaled_epsilon) const
{
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->polygons_covered_by_width(out, scaled_epsilon);
}

void ExtrusionMultiPath::polygons_covered_by_spacing(Polygons &out, const float scaled_epsilon) const
{
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->polygons_covered_by_spacing(out, scaled_epsilon);
}

double ExtrusionMultiPath::min_mm3_per_mm() const
{
    double min_mm3_per_mm = std::numeric_limits<double>::max();
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        min_mm3_per_mm = std::min(min_mm3_per_mm, path->mm3_per_mm);
    return min_mm3_per_mm;
}

Polyline ExtrusionMultiPath::as_polyline() const
{
    size_t len = 0;
    for (size_t i_path = 0; i_path < paths.size(); ++ i_path) {
        assert(! paths[i_path].polyline.points.empty());
        assert(i_path == 0 || paths[i_path - 1].polyline.points.back() == paths[i_path].polyline.points.front());
        len += paths[i_path].polyline.points.size();
    }
    // The connecting points between the segments are equal.
    len -= paths.size() - 1;

    Polyline out;
    if (len > 0) {
        out.points.reserve(len);
        out.points.push_back(paths.front().polyline.points.front());
        for (size_t i_path = 0; i_path < paths.size(); ++ i_path)
            out.points.insert(out.points.end(), paths[i_path].polyline.points.begin() + 1, paths[i_path].polyline.points.end());
    }
    return out;
}

bool
ExtrusionLoop::make_clockwise()
{
    bool was_ccw = this->polygon().is_counter_clockwise();
    if (was_ccw) this->reverse();
    return was_ccw;
}

bool
ExtrusionLoop::make_counter_clockwise()
{
    bool was_cw = this->polygon().is_clockwise();
    if (was_cw) this->reverse();
    return was_cw;
}

void
ExtrusionLoop::reverse()
{
    for (ExtrusionPaths::iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->reverse();
    std::reverse(this->paths.begin(), this->paths.end());
}

Polygon
ExtrusionLoop::polygon() const
{
    Polygon polygon;
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path) {
        // for each polyline, append all points except the last one (because it coincides with the first one of the next polyline)
        polygon.points.insert(polygon.points.end(), path->polyline.points.begin(), path->polyline.points.end()-1);
    }
    return polygon;
}

double
ExtrusionLoop::length() const
{
    double len = 0;
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        len += path->polyline.length();
    return len;
}

bool
ExtrusionLoop::split_at_vertex(const Point &point)
{
    for (ExtrusionPaths::iterator path = this->paths.begin(); path != this->paths.end(); ++path) {
        int idx = path->polyline.find_point(point);
        if (idx != -1) {
            if (this->paths.size() == 1) {
                // just change the order of points
                path->polyline.points.insert(path->polyline.points.end(), path->polyline.points.begin() + 1, path->polyline.points.begin() + idx + 1);
                path->polyline.points.erase(path->polyline.points.begin(), path->polyline.points.begin() + idx);
            } else {
                // new paths list starts with the second half of current path
                ExtrusionPaths new_paths;
                new_paths.reserve(this->paths.size() + 1);
                {
                    ExtrusionPath p = *path;
                    p.polyline.points.erase(p.polyline.points.begin(), p.polyline.points.begin() + idx);
                    if (p.polyline.is_valid()) new_paths.push_back(p);
                }
            
                // then we add all paths until the end of current path list
                new_paths.insert(new_paths.end(), path+1, this->paths.end());  // not including this path
            
                // then we add all paths since the beginning of current list up to the previous one
                new_paths.insert(new_paths.end(), this->paths.begin(), path);  // not including this path
            
                // finally we add the first half of current path
                {
                    ExtrusionPath p = *path;
                    p.polyline.points.erase(p.polyline.points.begin() + idx + 1, p.polyline.points.end());
                    if (p.polyline.is_valid()) new_paths.push_back(p);
                }
                // we can now override the old path list with the new one and stop looping
                std::swap(this->paths, new_paths);
            }
            return true;
        }
    }
    return false;
}

void
ExtrusionLoop::split_at(const Point &point)
{
    if (this->paths.empty()) return;
    
    // find the closest path and closest point belonging to that path
    size_t path_idx = 0;
    Point p = this->paths.front().first_point();
    double min = point.distance_to(p);
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path) {
        Point p_tmp = point.projection_onto(path->polyline);
        double dist = point.distance_to(p_tmp);
        if (dist < min) {
            p = p_tmp;
            min = dist;
            path_idx = path - this->paths.begin();
        }
    }
    
    // now split path_idx in two parts
    const ExtrusionPath &path = this->paths[path_idx];
    ExtrusionPath p1(path.role, path.mm3_per_mm, path.width, path.height);
    ExtrusionPath p2(path.role, path.mm3_per_mm, path.width, path.height);
    path.polyline.split_at(p, &p1.polyline, &p2.polyline);
    
    if (this->paths.size() == 1) {
        if (! p1.polyline.is_valid())
            std::swap(this->paths.front().polyline.points, p2.polyline.points);
        else if (! p2.polyline.is_valid())
            std::swap(this->paths.front().polyline.points, p1.polyline.points);
        else {
            p2.polyline.points.insert(p2.polyline.points.end(), p1.polyline.points.begin() + 1, p1.polyline.points.end());
            std::swap(this->paths.front().polyline.points, p2.polyline.points);
        }
    } else {
        // install the two paths
        this->paths.erase(this->paths.begin() + path_idx);
        if (p2.polyline.is_valid()) this->paths.insert(this->paths.begin() + path_idx, p2);
        if (p1.polyline.is_valid()) this->paths.insert(this->paths.begin() + path_idx, p1);
    }
    
    // split at the new vertex
    this->split_at_vertex(p);
}

void
ExtrusionLoop::clip_end(double distance, ExtrusionPaths* paths) const
{
    *paths = this->paths;
    
    while (distance > 0 && !paths->empty()) {
        ExtrusionPath &last = paths->back();
        double len = last.length();
        if (len <= distance) {
            paths->pop_back();
            distance -= len;
        } else {
            last.polyline.clip_end(distance);
            break;
        }
    }
}

bool
ExtrusionLoop::has_overhang_point(const Point &point) const
{
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path) {
        int pos = path->polyline.find_point(point);
        if (pos != -1) {
            // point belongs to this path
            // we consider it overhang only if it's not an endpoint
            return (path->is_bridge() && pos > 0 && pos != (int)(path->polyline.points.size())-1);
        }
    }
    return false;
}

void ExtrusionLoop::polygons_covered_by_width(Polygons &out, const float scaled_epsilon) const
{
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->polygons_covered_by_width(out, scaled_epsilon);
}

void ExtrusionLoop::polygons_covered_by_spacing(Polygons &out, const float scaled_epsilon) const
{
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        path->polygons_covered_by_spacing(out, scaled_epsilon);
}

double
ExtrusionLoop::min_mm3_per_mm() const
{
    double min_mm3_per_mm = std::numeric_limits<double>::max();
    for (ExtrusionPaths::const_iterator path = this->paths.begin(); path != this->paths.end(); ++path)
        min_mm3_per_mm = std::min(min_mm3_per_mm, path->mm3_per_mm);
    return min_mm3_per_mm;
}

}
