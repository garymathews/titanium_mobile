/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2020 by Axway, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
package ti.modules.titanium.ui.widget.tableview;

import java.util.ArrayList;
import java.util.List;

import org.appcelerator.kroll.KrollDict;
import org.appcelerator.titanium.TiApplication;
import org.appcelerator.titanium.TiC;
import org.appcelerator.titanium.TiDimension;
import org.appcelerator.titanium.util.TiUIHelper;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RectShape;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.selection.ItemDetailsLookup;
import androidx.recyclerview.selection.ItemKeyProvider;
import androidx.recyclerview.selection.SelectionPredicates;
import androidx.recyclerview.selection.SelectionTracker;
import androidx.recyclerview.selection.StorageStrategy;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import ti.modules.titanium.ui.TableViewProxy;
import ti.modules.titanium.ui.TableViewRowProxy;
import ti.modules.titanium.ui.TableViewSectionProxy;
import ti.modules.titanium.ui.widget.TiSwipeRefreshLayout;
import ti.modules.titanium.ui.widget.listview.ItemTouchHandler;
import ti.modules.titanium.ui.widget.listview.TiNestedRecyclerView;
import ti.modules.titanium.ui.widget.searchbar.TiUISearchBar.OnSearchChangeListener;

public class TiTableView extends TiSwipeRefreshLayout implements OnSearchChangeListener
{
	private static final String TAG = "TiTableView";

	private static final int CACHE_SIZE = 48;
	private static final int PRELOAD_SIZE = CACHE_SIZE * 2;

	private final TableViewAdapter adapter;
	private final DividerItemDecoration decoration;
	private final TableViewProxy proxy;
	private final TiNestedRecyclerView recyclerView;
	private final List<TableViewRowProxy> rows = new ArrayList<>();
	private final SelectionTracker tracker;

	private boolean isFiltered = false;
	private int scrollOffsetX = 0;
	private int scrollOffsetY = 0;

	public TiTableView(TableViewProxy proxy)
	{
		super(proxy.getActivity());

		this.proxy = proxy;

		this.recyclerView = new TiNestedRecyclerView(getContext());
		this.recyclerView.setFocusable(true);
		this.recyclerView.setFocusableInTouchMode(true);
		this.recyclerView.setBackgroundColor(Color.TRANSPARENT);
		this.recyclerView.setLayoutManager(new LinearLayoutManager(getContext()));

		// Add listener to fire scroll events.
		this.recyclerView.addOnScrollListener(new RecyclerView.OnScrollListener()
		{
			@Override
			public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState)
			{
				super.onScrollStateChanged(recyclerView, newState);

				if (newState == RecyclerView.SCROLL_STATE_IDLE) {
					final KrollDict payload = generateScrollPayload();
					final TiNestedRecyclerView nestedRecyclerView = getRecyclerView();

					// Obtain last touch position for `scrollend` event.
					final TiDimension xDimension =
						new TiDimension(nestedRecyclerView.getLastTouchX(), TiDimension.TYPE_WIDTH);
					final TiDimension yDimension =
						new TiDimension(nestedRecyclerView.getLastTouchY(), TiDimension.TYPE_HEIGHT);
					payload.put(TiC.EVENT_PROPERTY_X, xDimension.getAsDefault(nestedRecyclerView));
					payload.put(TiC.EVENT_PROPERTY_Y, yDimension.getAsDefault(nestedRecyclerView));

					proxy.fireSyncEvent(TiC.EVENT_SCROLLEND, payload);
				}
			}

			@Override
			public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy)
			{
				super.onScrolled(recyclerView, dx, dy);

				// Update scroll offsets.
				scrollOffsetX += dx;
				scrollOffsetY += dy;

				final KrollDict payload = generateScrollPayload();

				proxy.fireSyncEvent(TiC.EVENT_SCROLL, payload);
			}
		});

		// Disable table animations.
		this.recyclerView.setItemAnimator(null);

		// Optimize scroll performance.
		recyclerView.setItemViewCacheSize(CACHE_SIZE);

		// Set list separator.
		decoration = new DividerItemDecoration(getContext(), DividerItemDecoration.VERTICAL);
		this.recyclerView.addItemDecoration(decoration);

		this.adapter = new TableViewAdapter(getContext(), this.rows);
		this.recyclerView.setAdapter(this.adapter);

		// Create ItemTouchHelper for swipe-to-delete and move gestures.
		final ItemTouchHandler itemTouchHandler = new ItemTouchHandler(this.adapter, this.proxy, this.recyclerView);
		final ItemTouchHelper itemTouchHelper = new ItemTouchHelper(itemTouchHandler);
		itemTouchHelper.attachToRecyclerView(this.recyclerView);

		// Fire `postlayout` on layout changes.
		this.addOnLayoutChangeListener(new OnLayoutChangeListener()
		{
			@Override
			public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop,
									   int oldRight, int oldBottom)
			{
				TiUIHelper.firePostLayoutEvent(proxy);
			}
		});

		// TODO: Implement native item selection.
		this.tracker = new SelectionTracker.Builder("table_view_selection",
			this.recyclerView,
			new ItemKeyProvider(1)
			{
				@Nullable
				@Override
				public Object getKey(int position)
				{
					return rows.get(position);
				}

				@Override
				public int getPosition(@NonNull Object key)
				{
					return rows.indexOf(key);
				}
			},
			new ItemDetailsLookup()
			{
				@Nullable
				@Override
				public ItemDetails getItemDetails(@NonNull MotionEvent e)
				{
					final View view = recyclerView.findChildViewUnder(e.getX(), e.getY());
					if (view != null) {
						final TableViewHolder holder = (TableViewHolder) recyclerView.getChildViewHolder(view);
						return new ItemDetails()
						{
							@Override
							public int getPosition()
							{
								return holder.getAdapterPosition();
							}

							@Nullable
							@Override
							public Object getSelectionKey()
							{
								return rows.get(getPosition());
							}
						};
					}
					return null;
				}
			},
			StorageStrategy.createLongStorage()
		)
			.withSelectionPredicate(SelectionPredicates.<Long>createSelectSingleAnything())
			.build();
		this.tracker.addObserver(new SelectionTracker.SelectionObserver() {

			@Override
			public void onSelectionChanged()
			{
				super.onSelectionChanged();

				/*if (tracker.hasSelection()) {
					final Iterator<TableViewRowProxy> i = tracker.getSelection().iterator();
					while (i.hasNext()) {
						final TableViewRowProxy proxy = i.next();
						// Log.d(TAG, "SELECTED: " + proxy.getProperties().getString(TiC.PROPERTY_TITLE));
					}
				}*/
			}
		});
		this.adapter.setTracker(this.tracker);

		// Disable pull-down refresh support until a Titanium "RefreshControl" has been assigned.
		setSwipeRefreshEnabled(false);

		addView(this.recyclerView);
	}

	/**
	 * Filter current rows with query string.
	 *
	 * @param query String to query rows with.
	 */
	@Override
	public void filterBy(String query)
	{
		if (query == null || query.isEmpty()) {

			// No query, update adapter with original items.
			update();
			this.isFiltered = false;
			return;
		}

		update(query);
		this.isFiltered = true;
	}

	/**
	 * Generate payload for `scroll` and `scrollend` events.
	 *
	 * @return KrollDict
	 */
	public KrollDict generateScrollPayload()
	{
		final KrollDict payload = new KrollDict();
		final LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();

		// Obtain index for first visible row.
		final View firstVisibleView =
			layoutManager.findViewByPosition(layoutManager.findFirstVisibleItemPosition());
		final TableViewHolder firstVisibleHolder =
			(TableViewHolder) recyclerView.getChildViewHolder(firstVisibleView);
		final TableViewRowProxy firstVisibleProxy = (TableViewRowProxy) firstVisibleHolder.getProxy();
		final int firstVisibleIndex = firstVisibleProxy.getIndexInSection();
		payload.put(TiC.PROPERTY_FIRST_VISIBLE_ITEM, firstVisibleIndex);

		// Obtain scroll offset for content.
		final KrollDict contentOffset = new KrollDict();
		final TiDimension scrollOffsetXDimension = new TiDimension(scrollOffsetX, TiDimension.TYPE_WIDTH);
		final TiDimension scrollOffsetYDimension = new TiDimension(scrollOffsetY, TiDimension.TYPE_HEIGHT);
		contentOffset.put(TiC.EVENT_PROPERTY_X, scrollOffsetXDimension.getAsDefault(recyclerView));
		contentOffset.put(TiC.EVENT_PROPERTY_Y, scrollOffsetYDimension.getAsDefault(recyclerView));
		payload.put(TiC.PROPERTY_CONTENT_OFFSET, contentOffset);

		// Approximate content size.
		// NOTE: Due to recycling of views, we cannot calculate the true
		// content size without loading all rows. The best we can do is an
		// approximation based on first visible row.
		final KrollDict contentSize = new KrollDict();
		final TiDimension contentWidthDimension =
			new TiDimension(firstVisibleView.getMeasuredWidth(), TiDimension.TYPE_WIDTH);
		final TiDimension contentHeightDimension =
			new TiDimension(firstVisibleView.getMeasuredHeight() * rows.size(), TiDimension.TYPE_HEIGHT);
		contentSize.put(TiC.PROPERTY_WIDTH, contentWidthDimension.getAsDefault(recyclerView));
		contentSize.put(TiC.PROPERTY_HEIGHT, contentHeightDimension.getAsDefault(recyclerView));
		payload.put(TiC.PROPERTY_CONTENT_SIZE, contentSize);

		// Obtain view size.
		final KrollDict size = new KrollDict();
		final TiDimension widthDimension =
			new TiDimension(recyclerView.getMeasuredWidth(), TiDimension.TYPE_WIDTH);
		final TiDimension heightDimension =
			new TiDimension(recyclerView.getMeasuredHeight(), TiDimension.TYPE_HEIGHT);
		size.put(TiC.PROPERTY_WIDTH, widthDimension.getAsDefault(recyclerView));
		size.put(TiC.PROPERTY_HEIGHT, heightDimension.getAsDefault(recyclerView));
		payload.put(TiC.PROPERTY_SIZE, size);

		return payload;
	}

	/**
	 * Get table adapter.
	 *
	 * @return TableViewAdapter
	 */
	public TableViewAdapter getAdapter()
	{
		return this.adapter;
	}

	/**
	 * Get recycler view of table.
	 *
	 * @return TiNestedRecyclerView
	 */
	public TiNestedRecyclerView getRecyclerView()
	{
		return this.recyclerView;
	}

	/**
	 * Obtain row for specified index.
	 *
	 * @param index Index of row.
	 * @return TableViewRowProxy
	 */
	public TableViewRowProxy getRowByIndex(int index)
	{
		for (TableViewRowProxy row : this.rows) {
			if (row.index == index) {
				return row;
			}
		}
		return null;
	}

	/**
	 * Obtain adapter index from list item index.
	 *
	 * @param index List item index.
	 * @return Integer of adapter index.
	 */
	public int getAdapterIndex(int index)
	{
		for (TableViewRowProxy row : this.rows) {
			if (row.index == index) {
				return this.rows.indexOf(row);
			}
		}
		return -1;
	}

	/**
	 * Obtain row from adapter index.
	 *
	 * @param index List item adapter index.
	 * @return Row at specified adapter index.
	 */
	public TableViewRowProxy getAdapterItem(int index)
	{
		return this.rows.get(index);
	}

	/**
	 * Determine if table results are filtered by query.
	 *
	 * @return Boolean
	 */
	public boolean isFiltered()
	{
		return this.isFiltered;
	}

	/**
	 * Release models.
	 */
	public void release()
	{
		for (TableViewRowProxy row : this.rows) {
			row.releaseViews();
		}
		this.rows.clear();
	}

	/**
	 * Set row separator color and height.
	 *
	 * @param color Color of separator.
	 * @param height Height of separator.
	 */
	public void setSeparator(int color, int height)
	{
		final ShapeDrawable separator = new ShapeDrawable(new RectShape());

		separator.setIntrinsicHeight(height);
		separator.getPaint().setColor(color);
		decoration.setDrawable(separator);

		this.recyclerView.invalidate();
	}

	/**
	 * Set row separator drawable.
	 *
	 * @param drawable Separator drawable.
	 */
	public void setSeparator(Drawable drawable)
	{
		decoration.setDrawable(drawable);
	}

	public void update()
	{
		this.update(null);
	}

	/**
	 * Update table rows, including headers and footers.
	 */
	public void update(String query)
	{
		final KrollDict properties = this.proxy.getProperties();
		final boolean hasHeader = properties.containsKeyAndNotNull(TiC.PROPERTY_HEADER_TITLE)
			|| properties.containsKeyAndNotNull(TiC.PROPERTY_HEADER_VIEW);
		final boolean hasFooter = properties.containsKeyAndNotNull(TiC.PROPERTY_FOOTER_TITLE)
			|| properties.containsKeyAndNotNull(TiC.PROPERTY_FOOTER_VIEW);

		final boolean caseInsensitive = properties.optBoolean(TiC.PROPERTY_FILTER_CASE_INSENSITIVE, true);
		final boolean filterAnchored = properties.optBoolean(TiC.PROPERTY_FILTER_ANCHORED, false);
		final String filterAttribute = properties.optString(TiC.PROPERTY_FILTER_ATTRIBUTE, TiC.PROPERTY_TITLE);

		if (query != null && caseInsensitive) {
			query = query.toLowerCase();
		}

		// Clear current models.
		this.rows.clear();

		// Add placeholder item for TableView header.
		if (hasHeader) {
			final TableViewRowProxy row = new TableViewRowProxy(true);

			row.getProperties().put(TiC.PROPERTY_HEADER_TITLE, properties.get(TiC.PROPERTY_HEADER_TITLE));
			row.getProperties().put(TiC.PROPERTY_HEADER_VIEW, properties.get(TiC.PROPERTY_HEADER_VIEW));
			row.setParent(this.proxy);

			this.rows.add(row);
		}

		// Iterate through data, processing each supported entry.
		for (final Object entry : proxy.getData()) {

			if (entry instanceof TableViewSectionProxy) {
				final TableViewSectionProxy section = (TableViewSectionProxy) entry;
				final TableViewRowProxy[] rows = section.getRows();

				// Add placeholder item for TableViewSection header/footer.
				if (rows.length == 0 && (section.hasHeader() || section.hasFooter())) {
					final TableViewRowProxy row = new TableViewRowProxy(true);

					row.setParent(section);
					this.rows.add(row);
				}

				int filteredIndex = 0;
				for (int i = 0; i < rows.length; i++) {
					final TableViewRowProxy row = rows[i];

					// Handle search query.
					if (query != null) {
						String attribute = row.getProperties().optString(filterAttribute, null);

						if (attribute != null) {
							if (caseInsensitive) {
								attribute = attribute.toLowerCase();
							}

							if (!((filterAnchored && attribute.startsWith(query))
								|| (!filterAnchored && attribute.contains(query)))) {
								continue;
							}
						}
					}

					// Update filtered index of row.
					row.setFilteredIndex(query != null ? filteredIndex++ : -1);

					this.rows.add(row);
				}

				// Update section filtered row count.
				section.setFilteredRowCount(query != null ? filteredIndex : -1);
			}
		}

		// Add placeholder item for TableView footer.
		if (hasFooter) {
			final TableViewRowProxy row = new TableViewRowProxy(true);

			row.getProperties().put(TiC.PROPERTY_FOOTER_TITLE, properties.get(TiC.PROPERTY_FOOTER_TITLE));
			row.getProperties().put(TiC.PROPERTY_FOOTER_VIEW, properties.get(TiC.PROPERTY_FOOTER_VIEW));
			row.setParent(this.proxy);

			this.rows.add(row);
		}

		// Pre-load views for smooth initial scroll.
		final int preloadSize = Math.min(this.rows.size(), PRELOAD_SIZE);
		for (int i = 0; i < preloadSize; i++) {
			this.rows.get(i).getOrCreateView();
		}

		// Update models.
		updateModels();
	}

	/**
	 * Update table models (rows) index and notify adapter.
	 */
	public void updateModels()
	{
		int i = 0;
		for (TableViewRowProxy row : this.rows) {
			if (row.isPlaceholder()) {
				continue;
			}

			// Update row index, ignoring placeholder entries.
			row.index = i++;
		}

		final Activity activity = TiApplication.getAppCurrentActivity();
		final View previousFocus = activity != null ? activity.getCurrentFocus() : null;

		// Notify the adapter of changes.
		this.adapter.notifyDataSetChanged();

		// FIXME: This is not an ideal workaround for an issue where recycled rows that were in focus
		//        lose their focus when the data set changes. There are improvements to be made here.
		//        This can be reproduced when setting a Ti.UI.TextField in the Ti.UI.TableView.headerView for search.
		new Handler().post(new Runnable()
		{
			public void run()
			{
				final View currentFocus = activity != null ? activity.getCurrentFocus() : null;

				if (previousFocus != null && currentFocus != previousFocus) {

					// Request focus on previous component before dataset changed.
					previousFocus.requestFocus();
				}
			}
		});
	}
}
