# Markdown in gweni

A **MarkdownView** shows *CommonMark* with GFM's extras: ***bold italic***, ~~struck through~~, `inline code`, [a link](https://github.com/caseymcc/gweni) and an autolink, https://example.com/docs.  
A hard break ends the line before this one, and a soft
break joins this one to it.

- Lists, unordered
  - with a marker for each depth
    - nested as deep as needed
- and ordered:
  1. first
  2. second

```cpp
int main()
{
    return render("a line of code long enough to scroll sideways in a narrow view");
}
```

| Element | Status | Count |
|:--------|:------:|------:|
| Tables | aligned | 3 |
| Cells | **styled** | 12 |

> A block quote, drawn with a bar.
> > And one nested in it.

---

## Headings

### Level three
#### Level four
##### Level five
###### Level six

    an indented code block
      keeps its spacing

An image, ![a diagram of the blocks](diagram.png), shows its alt text, and <span>inline HTML</span> stays as written:

<div class="note">
  A raw HTML block, literal.
</div>

The last paragraph, which an edit changes.
