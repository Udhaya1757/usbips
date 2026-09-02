#pragma once

namespace WindowsService {

// Returns 1 when launched interactively, 0 after service dispatch, or -1 on error.
int Dispatch();

}
