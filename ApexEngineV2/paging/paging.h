#ifndef PAGING_H
#define PAGING_H

namespace apex {
namespace paging {

enum PageState {
    LOADED,
    UNLOADING,
    UNLOADED
};

class GridTile {

};

class Patch {
public:
    GridTile m_grid_tile;
    PageState m_page_state;

    Patch()
    {
    }
};

}
}

#endif