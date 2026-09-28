/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

namespace Hyperion {

struct X11Atoms
{
    ::Atom wmProtocols = 0;
    ::Atom wmDeleteWindow = 0;
    ::Atom netWmName = 0;
    ::Atom utf8String = 0;
};

::Display* X11_AcquireDisplay();
void X11_ReleaseDisplay();

const X11Atoms& X11_GetAtoms();

} // namespace Hyperion
