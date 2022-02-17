#include "octree.h"

#include <iomanip>

namespace hyperion {
std::ostream &operator<<(std::ostream &out, const Octree &octree) // output
{
    std::string tab_string;

    for (int i = 0; i < octree.m_level; i++) {
        tab_string += "  ";
    }

    out << tab_string << "Octree at level " << octree.m_level << "\n";
    out << tab_string << "All empty? " << octree.AllEmpty() << "\n";
    out << tab_string << octree.GetNodes().size() << " direct nodes: [";

    for (size_t i = 0; i < octree.GetNodes().size(); i++) {
        out << octree.GetNodes()[i].m_id;

        if (i != octree.GetNodes().size() - 1) {
            out << ", ";
        }
    }

    out << "]\n";

    out << tab_string << "Octants:\n";
    if (!octree.IsDivided()) {
        out << tab_string << "<no octants>";
    } else {
        for (int x = 0; x < 2; x++) {
            for (int y = 0; y < 2; y++) {
                for (int z = 0; z < 2; z++) {
                    int index = 4 * x + 2 * y + z;

                    out << tab_string << "[" << x << ", " << y << ", " << z << "]:\n";
                    out << *octree.GetOctants()[index].m_octree << "\n\n";
                }
            }
        }
    }

    return out;
}
} // namespace hyperion