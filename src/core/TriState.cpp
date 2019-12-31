static int TriState::indexFromTriState(TriState::State triState)
{
    switch (triState) {
    case TriState::Inherit:
        return 0;
    case TriState::Enable:
        return 1;
    case TriState::Disable:
        return 2;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

static TriState::State TriTriState::StateFromIndex(int index)
{
    switch (index) {
    case 0:
        return TriState::Inherit;
    case 1:
        return TriState::Enable;
    case 2:
        return TriState::Disable;
    default:
        Q_ASSERT(false);
        return TriState::Inherit;
    }
}
