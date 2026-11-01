# 性能与闪烁优化

这一页记录做 ICMapStudio 时踩到的 WinTk 性能问题，以及框架里对应的修复。基准数字来自当时的机器，只用于判断数量级，不是所有机器的绝对保证。

## 1. 大列表：不要逐条 `wtk_listbox_add`

原生 `LISTBOX` 的 `LB_ADDSTRING` 本身就不便宜；逐条调用还会每条触发一次无效化。两万条的量级下，UI 会明显卡顿。

```c
/* 不推荐：数万条时每条一次重绘 */
for (i = 0; i < n; i++)
    wtk_listbox_add(list, items[i]);

/* 推荐：一次提交，只刷一次 */
wtk_listbox_add_many(list, items, n);
```

需要更极端的规模时用**虚拟列表**：

```c
typedef void (*WtkListItemFn)(WtkWidget *list, int index, HDC dc,
                              const RECT *rc, void *userdata);

void draw_item(WtkWidget *list, int index, HDC dc,
               const RECT *rc, void *userdata)
{
    WCHAR text[128];
    RECT row = *rc;
    _snwprintf(text, 127, L"第 %d 行", index);
    DrawTextW(dc, text, -1, &row,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/* count 可以到几十万；控件只记数量，只画可见行 */
wtk_listbox_set_virtual(list, n, draw_item, NULL);
wtk_listbox_virtual_refresh(list); /* 数据变化后 */
```

框架内部使用 `LBS_OWNERDRAWFIXED | LBS_NODATA` + `LB_SETCOUNT`，建立是 O(1)；选中背景和焦点框由框架绘制。

历史基准：

| 场景 | 结果 |
|---|---|
| 100,000 条虚拟列表 | 建立约 4 ms，整屏重绘约 2 ms |
| 1,000,000 条虚拟列表 | 建立约 3 ms，整屏重绘约 2 ms（只画约 24 行） |
| 18,622 条 Lua API | 虚拟列表建立约 0 ms |

## 2. 批量更新：`bulk_begin/end`

文本追加、列表填充、日志刷新经常在循环里做。把循环包起来，期间不重绘，结束时只刷一次：

```c
wtk_widget_bulk_begin(text);
for (i = 0; i < n; i++)
    wtk_text_append(text, lines[i]);
wtk_widget_bulk_end(text);
```

- 可嵌套；必须成对调用。
- 内部使用 `WM_SETREDRAW` + 一次 `InvalidateRect`/`UpdateWindow`。
- 不要跨 UI 消息长时间保持 bulk 状态。

历史基准：

| 场景 | 普通 | bulk |
|---|---|---|
| 文本 400 次追加 | 约 6610 ms | 约 225 ms |
| listbox 20,000 条 | 约 5255 ms | 约 980 ms |

## 3. 大文本：32K 限制和追加

原生多行 `EDIT` 默认只接受约 32K 字符，较大的 JSON/日志会被静默截断。WinTk 创建 Entry/Text 时已把 `EM_SETLIMITTEXT` 提高到 `0x7FFFFFFE`。

追加文本不要每次重设全文：

```c
/* 不要：文本越大越慢 */
wtk_text_set_text(text, whole_buffer);

/* 推荐 */
wtk_text_append(text, line);
```

`wtk_text_set_text` 也会在内容未变化时直接返回，避免无谓重排。

## 4. 闪烁

- 顶层窗口**不使用** `WS_EX_COMPOSITED`。它会在任意子控件重绘时重新合成整个窗口，鼠标 hover 都会造成整窗闪烁。
- 拖动布局时，`wtk_layout.c` 用 `BeginDeferWindowPos`/`DeferWindowPos`/`EndDeferWindowPos` 批量提交子窗口位置，带 `SWP_NOREDRAW`，最后统一 `RedrawWindow(... RDW_ALLCHILDREN ...)`。
- 初始隐藏布局不走 `SWP_NOREDRAW`，避免“窗口显示后一片空白”的回归。
- 各控件自身的双缓冲和 `WM_ERASEBKGND` 返回 1 仍然是第一道防线。

## 5. 树控件

`wtk_tree` 基于原生 `SysTreeView32`，节点索引在 `wtk_tree_clear()` 前稳定。折叠已选中的节点时 Windows 可能把选择移到父节点，这是原生行为；应用应该用节点索引/类型判断，不要只凭“选中变化”就切换视图。

树节点的 `HTREEITEM` 数组由框架管理，随控件销毁释放。

## 6. 相关 API

- `wtk_listbox_add_many`
- `wtk_listbox_set_virtual` / `wtk_listbox_virtual_refresh` / `wtk_listbox_virtual_count`
- `wtk_widget_bulk_begin` / `wtk_widget_bulk_end`
- `wtk_text_append`
- `wtk_widget_input_hwnd`
- `wtk_tree`

详见 [API.md](API.md)。
