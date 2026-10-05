#!/bin/bash
set -e

REMOTE="${1:-origin}"

echo "=========================================================="
echo "    Pushing DriveSense to GitHub in Sequential Phases     "
echo "=========================================================="

echo -e "\n==> Phase 0: Pushing Initial Project Structure (chore: initial project structure)..."
git push "$REMOTE" fcb9ad9:refs/heads/main
sleep 1

echo -e "\n==> Phase 1 & 2: Pushing Stage 1-3 Documentation & Design Specs..."
git push "$REMOTE" feature/docs
git push "$REMOTE" a88bafe:refs/heads/develop
git push "$REMOTE" 0e9dc09:refs/heads/main
sleep 1

echo -e "\n==> Phase 3: Pushing Linux Kernel Driver (feat: character driver with timer)..."
git push "$REMOTE" feature/driver
git push "$REMOTE" f3ecb74:refs/heads/develop
sleep 1

echo -e "\n==> Phase 4: Pushing C++17 Dashboard Application (feat: ncurses dashboard)..."
git push "$REMOTE" feature/dashboard
git push "$REMOTE" 758cff2:refs/heads/develop
git push "$REMOTE" 98b6b5f:refs/heads/main
sleep 1

echo -e "\n==> Phase 5: Pushing Test Suites & QA Documentation (test: unit, integration, stress)..."
git push "$REMOTE" feature/testing
git push "$REMOTE" 3823221:refs/heads/develop
git push "$REMOTE" 0453261:refs/heads/main
sleep 1

echo -e "\n==> Phase 6: Pushing Final Delivery, README, Reports, and Release Tag v1.0..."
git push "$REMOTE" feature/final-delivery
git push "$REMOTE" develop
git push "$REMOTE" main
git push "$REMOTE" v1.0

echo -e "\n=========================================================="
echo "  SUCCESS: All phases, branches, and tags pushed to GitHub! "
echo "=========================================================="
