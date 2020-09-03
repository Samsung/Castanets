package android.support.v4.app;

import android.support.annotation.IdRes;
import android.support.annotation.Nullable;
import android.view.View;


/**
 * Callbacks to a {@link Fragment}'s container.
 */
public abstract class FragmentContainer {
    /**
     * Return the view with the given resource ID. May return {@code null} if the
     * view is not a child of this container.
     */
    @Nullable
    public abstract View onFindViewById(@IdRes int id);

    /**
     * Return {@code true} if the container holds any view.
     */
    public abstract boolean onHasView();
}
