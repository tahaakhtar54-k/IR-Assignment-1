import json
import re
import os
import tkinter as tk
from tkinter import ttk, messagebox

# Porter Stemmer
def is_vowel(w, i):
    c = w[i]
    if c in 'aeiou': return True
    if c == 'y' and i > 0: return not is_vowel(w, i - 1)
    return False

def measure(s):
    m, in_v = 0, False
    for i in range(len(s)):
        if is_vowel(s, i): in_v = True
        elif in_v: m += 1; in_v = False
    return m

def has_vowel(s):
    return any(is_vowel(s, i) for i in range(len(s)))

def ends_cvc(w):
    n = len(w)
    if n < 3: return False
    return (not is_vowel(w, n-3) and is_vowel(w, n-2) and
            not is_vowel(w, n-1) and w[n-1] not in 'wxy')

def has_suffix(w, sfx):
    if w.endswith(sfx): return w[:-len(sfx)]
    return None

def stem(word):
    w = word.lower()
    if len(w) <= 2: return w
    s = None

    if   (s := has_suffix(w, 'sses')) is not None: w = s + 'ss'
    elif (s := has_suffix(w, 'ies'))  is not None: w = s + 'i'
    elif not w.endswith('ss') and (s := has_suffix(w, 's')) is not None: w = s

    extra = False
    if   (s := has_suffix(w, 'eed')) is not None:
        if measure(s) > 0: w = s + 'ee'
    elif (s := has_suffix(w, 'ed'))  is not None:
        if has_vowel(s): w = s; extra = True
    elif (s := has_suffix(w, 'ing')) is not None:
        if has_vowel(s): w = s; extra = True
    if extra:
        if   has_suffix(w, 'at') is not None: w += 'e'
        elif has_suffix(w, 'bl') is not None: w += 'e'
        elif has_suffix(w, 'iz') is not None: w += 'e'
        else:
            n = len(w)
            if n > 1 and w[-1] == w[-2] and w[-1] not in 'lsz' and not is_vowel(w, n-1):
                w = w[:-1]
            elif measure(w) == 1 and ends_cvc(w):
                w += 'e'

    if (s := has_suffix(w, 'y')) is not None and has_vowel(s): w = s + 'i'

    for sfx, rep in [('ational','ate'),('tional','tion'),('enci','ence'),('anci','ance'),
                     ('izer','ize'),('abli','able'),('alli','al'),('entli','ent'),
                     ('eli','e'),('ousli','ous'),('ization','ize'),('ation','ate'),
                     ('ator','ate'),('alism','al'),('iveness','ive'),('fulness','ful'),
                     ('ousness','ous'),('aliti','al'),('iviti','ive'),('biliti','ble')]:
        if (s := has_suffix(w, sfx)) is not None and measure(s) > 0: w = s + rep; break

    for sfx, rep in [('icate','ic'),('ative',''),('alize','al'),('iciti','ic'),
                     ('ical','ic'),('ful',''),('ness','')]:
        if (s := has_suffix(w, sfx)) is not None and measure(s) > 0: w = s + rep; break

    for sfx in ['al','ance','ence','er','ic','able','ible','ant','ement','ment',
                'ent','ion','ou','ism','ate','iti','ous','ive','ize']:
        if (s := has_suffix(w, sfx)) is not None:
            if sfx == 'ion':
                if s and s[-1] in 'st' and measure(s) > 1: w = s; break
            elif measure(s) > 1: w = s; break

    if (s := has_suffix(w, 'e')) is not None:
        m = measure(s)
        if m > 1: w = s
        elif m == 1 and not ends_cvc(s): w = s

    if len(w) > 1 and w[-1] == 'l' and w[-2] == 'l' and measure(w) > 1: w = w[:-1]
    return w

# Load indices
def load_indices():
    base = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(base, 'inverted_index.json'),  'r') as f: index     = json.load(f)
    with open(os.path.join(base, 'positional_index.json'),'r') as f: positional= json.load(f)
    with open(os.path.join(base, 'doc_map.json'),          'r') as f: docmap    = json.load(f)
    return index, positional, docmap


def intersect(a, b):
    r, i, j = [], 0, 0
    while i < len(a) and j < len(b):
        if   a[i] == b[j]: r.append(a[i]); i += 1; j += 1
        elif a[i] <  b[j]: i += 1
        else:               j += 1
    return r

def union(a, b):
    r, i, j = [], 0, 0
    while i < len(a) and j < len(b):
        if   a[i] == b[j]: r.append(a[i]); i += 1; j += 1
        elif a[i] <  b[j]: r.append(a[i]); i += 1
        else:               r.append(b[j]); j += 1
    r.extend(a[i:]); r.extend(b[j:])
    return r

def get_index(index, w):
    return index.get(w, [])

def not_query(index, docmap, term):
    not_set = set(get_index(index, stem(term)))
    return sorted([int(d) for d in docmap if int(d) not in not_set])

def proximity_query(positional, t1, t2, k):
    result = []
    docs1 = positional.get(t1, {})
    docs2 = positional.get(t2, {})
    for doc in set(docs1) & set(docs2):
        for p1 in docs1[doc]:
            for p2 in docs2[doc]:
                if abs(p1 - p2) <= k + 1:
                    result.append(int(doc)); break
            else: continue
            break
    return sorted(result)

def process_query(query, index, positional, docmap):
    query = query.strip()
    tokens = query.split()
    all_docs = sorted([int(d) for d in docmap])

    # Parentheses
    if '(' in query:
        m = re.search(r'\((.+?)\)', query)
        inner = m.group(1).strip()
        inner_result = process_query(inner, index, positional, docmap)
        before = query[:query.index('(')].strip().split()

        if len(before) == 1 and before[0].upper() == 'NOT':
            s = set(inner_result)
            return [d for d in all_docs if d not in s]
        elif len(before) == 2:
            t1 = stem(before[0]); op = before[1].upper()
            if op == 'AND': return intersect(get_index(index, t1), inner_result)
            else:           return union(get_index(index, t1), inner_result)

    # Proximity
    if any(t.startswith('/') for t in tokens):
        t1 = stem(tokens[0]); t2 = stem(tokens[1]); k = int(tokens[2][1:])
        return proximity_query(positional, t1, t2, k)

    # Single term
    if len(tokens) == 1:
        return get_index(index, stem(tokens[0]))

    # NOT t1
    if len(tokens) == 2 and tokens[0].upper() == 'NOT':
        return not_query(index, docmap, tokens[1])

    # t1 t2 (implicit OR)
    if len(tokens) == 2:
        return union(get_index(index, stem(tokens[0])), get_index(index, stem(tokens[1])))

    # t1 OP t2
    if len(tokens) == 3:
        t1, op, t2 = stem(tokens[0]), tokens[1].upper(), stem(tokens[2])
        if op == 'AND': return intersect(get_index(index, t1), get_index(index, t2))
        else:           return union(get_index(index, t1), get_index(index, t2))

    # t1 OP NOT t2
    if len(tokens) == 4 and tokens[2].upper() == 'NOT':
        t1 = stem(tokens[0]); op = tokens[1].upper(); t2 = tokens[3]
        if op == 'AND': return intersect(get_index(index, t1), not_query(index, docmap, t2))
        else:           return union(get_index(index, t1), not_query(index, docmap, t2))

    # NOT t1 OP t2
    if len(tokens) == 4 and tokens[0].upper() == 'NOT':
        t1 = tokens[1]; op = tokens[2].upper(); t2 = stem(tokens[3])
        if op == 'AND': return intersect(not_query(index, docmap, t1), get_index(index, t2))
        else:           return union(not_query(index, docmap, t1), get_index(index, t2))

    # t1 OP t2 OP t3
    if len(tokens) == 5:
        t1, op1, t2, op2, t3 = stem(tokens[0]), tokens[1].upper(), stem(tokens[2]), tokens[3].upper(), stem(tokens[4])
        temp = intersect(get_index(index, t1), get_index(index, t2)) if op1 == 'AND' else union(get_index(index, t1), get_index(index, t2))
        return intersect(temp, get_index(index, t3)) if op2 == 'AND' else union(temp, get_index(index, t3))

    return []

# GUI
class IRApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Boolean Retrieval Model")
        self.root.geometry("1000x680")
        self.root.configure(bg="#1a1a2e")
        self.root.resizable(True, True)

        self.BG      = "#16213e"  
        self.SURFACE = "#0f3460"  
        self.BORDER  = "#533483"  
        self.ACCENT  = "#e94560"  
        self.SECONDARY = "#a8d8ea"  
        self.TEXT    = "#ffffff" 
        self.MUTED   = "#c4c4c4" 
        self.FAIL    = "#ff6b6b"  
        self.PASS    = "#4ecdc4" 
        self.CARD    = "#1a1a2e"  
        self.HIGHLIGHT = "#feca57"  

        # Load data
        try:
            self.index, self.positional, self.docmap = load_indices()
        except FileNotFoundError as e:
            messagebox.showerror("Error", f"Index file not found:\n{e}\n\nRun preprocessing.exe first.")
            root.destroy(); return

        self.history = []
        self._build_ui()

    def _build_ui(self):
        # Header
        hdr = tk.Frame(self.root, bg=self.SURFACE, height=64)
        hdr.pack(fill='x', side='top')
        hdr.pack_propagate(False)

        tk.Label(hdr, text="🔍 IR", bg=self.ACCENT, fg="#ffffff",
                 font=("Segoe UI", 16, "bold"),
                 width=4, relief='raised', bd=2).pack(side='left', padx=(20,12), pady=14)

        tk.Label(hdr, text="Boolean Retrieval System",
                 bg=self.SURFACE, fg=self.TEXT,
                 font=("Segoe UI", 14, "bold")).pack(side='left')

        tk.Label(hdr, text=f"  📄 {len(self.docmap)} documents loaded",
                 bg=self.SURFACE, fg=self.SECONDARY,
                 font=("Segoe UI", 10)).pack(side='left')

        tk.Label(hdr, text="v2.0 ✨",
                 bg=self.SURFACE, fg=self.HIGHLIGHT,
                 font=("Segoe UI", 10, "bold"),
                 relief='solid', bd=2, padx=8).pack(side='right', padx=20)

        # Main area
        body = tk.Frame(self.root, bg=self.BG)
        body.pack(fill='both', expand=True)

        # Left panel
        left = tk.Frame(body, bg=self.SURFACE, width=300)
        left.pack(side='left', fill='y', padx=(12,0), pady=12)
        left.pack_propagate(False)

        # Right panel
        right = tk.Frame(body, bg=self.BG)
        right.pack(side='left', fill='both', expand=True, padx=12, pady=12)

        self._build_left(left)
        self._build_right(right)

    def _lbl(self, parent, text, size=10, color=None, bold=False):
        font = ("Segoe UI", size, "bold" if bold else "normal")
        tk.Label(parent, text=text, bg=parent['bg'],
                 fg=color or self.SECONDARY,
                 font=font).pack(anchor='w', padx=14, pady=(10,2))

    def _build_left(self, parent):
        # Quick insert
        self._lbl(parent, "QUICK INSERT", color=self.MUTED)
        chips_frame = tk.Frame(parent, bg=self.SURFACE)
        chips_frame.pack(fill='x', padx=12, pady=(0,8))
        chips = [(" AND ", " AND "), (" OR ", " OR "), ("NOT ", "NOT "),
                 ("( )", "( )"), ("/1", " /1"), ("/2", " /2"), ("/3", " /3")]
        for idx, (label, text) in enumerate(chips):
            tk.Button(chips_frame, text=label, bg=self.SECONDARY, fg=self.BG,
                      font=("Segoe UI", 9, "bold"), relief='raised', bd=2,
                      padx=8, pady=4, cursor='hand2',
                      command=lambda t=text: self._insert(t)).grid(row=idx//4, column=idx%4, padx=3, pady=3, sticky='w')

        # Query section
        self._lbl(parent, "SEARCH QUERY", color=self.MUTED)
        ta_frame = tk.Frame(parent, bg=self.BORDER, bd=0)
        ta_frame.pack(fill='x', padx=12, pady=(0,4))

        self.query_input = tk.Text(ta_frame, height=4, bg=self.CARD, fg=self.TEXT,
                                   insertbackground=self.HIGHLIGHT,
                                   font=("Segoe UI", 12),
                                   relief='sunken', bd=3, wrap='word',
                                   selectbackground=self.ACCENT,
                                   selectforeground="#ffffff")
        self.query_input.pack(fill='x')
        self.query_input.bind('<Return>', lambda e: (self._run_query(), 'break'))

        # Placeholder
        self._set_placeholder()

        # Search button
        btn = tk.Button(parent, text="🔎 SEARCH",
                        bg=self.ACCENT, fg="#ffffff",
                        font=("Segoe UI", 12, "bold"),
                        relief='raised', bd=3, pady=10,
                        activebackground="#c0392b", activeforeground="#ffffff",
                        cursor='hand2', command=self._run_query)
        btn.pack(fill='x', padx=12, pady=(0,12))

        # Syntax reference
        self._lbl(parent, "SYNTAX REFERENCE", color=self.MUTED)
        syntax_frame = tk.Frame(parent, bg=self.CARD, bd=0)
        syntax_frame.pack(fill='x', padx=12, pady=(0,8))
        rows = [("term",              "Single term"),
                ("t1 AND t2",         "Both must appear"),
                ("t1 OR t2",          "Either term"),
                ("NOT t1",            "Exclude term"),
                ("t1 AND NOT t2",     "t1 without t2"),
                ("t1 AND (t2 OR t3)", "Grouped"),
                ("t1 t2 /k",          "Within k words")]
        for code, desc in rows:
            r = tk.Frame(syntax_frame, bg=self.CARD)
            r.pack(fill='x', padx=8, pady=2)
            tk.Label(r, text=code,  bg=self.CARD, fg=self.HIGHLIGHT,
                     font=("Segoe UI", 9, "bold"), width=18, anchor='w').pack(side='left')
            tk.Label(r, text=desc,  bg=self.CARD, fg=self.MUTED,
                     font=("Segoe UI", 9), anchor='w').pack(side='left')

        # History
        self._lbl(parent, "RECENT QUERIES", color=self.MUTED)
        self.hist_frame = tk.Frame(parent, bg=self.SURFACE)
        self.hist_frame.pack(fill='both', expand=True, padx=12)

    def _build_right(self, parent):
        # Stats bar
        self.stats_bar = tk.Frame(parent, bg=self.SURFACE, height=52)
        self.stats_bar.pack(fill='x', pady=(0,10))
        self.stats_bar.pack_propagate(False)

        self.stat_count = tk.Label(self.stats_bar, text="—",
                                   bg=self.SURFACE, fg=self.HIGHLIGHT,
                                   font=("Segoe UI", 24, "bold"))
        self.stat_count.pack(side='left', padx=(16,4), pady=8)

        tk.Label(self.stats_bar, text="📊 DOCUMENTS",
                 bg=self.SURFACE, fg=self.SECONDARY,
                 font=("Segoe UI", 10, "bold")).pack(side='left', padx=(0,16), anchor='s', pady=14)

        self.stat_query = tk.Label(self.stats_bar, text="💭 Run a query to see results",
                                   bg=self.SURFACE, fg=self.MUTED,
                                   font=("Segoe UI", 11))
        self.stat_query.pack(side='left')

        # Results area with scrollbar
        container = tk.Frame(parent, bg=self.BG)
        container.pack(fill='both', expand=True)

        scrollbar = tk.Scrollbar(container, bg=self.SURFACE, troughcolor=self.BG,
                                 relief='flat', bd=0)
        scrollbar.pack(side='right', fill='y')

        self.results_list = tk.Listbox(
            container,
            bg=self.CARD, fg=self.TEXT,
            font=("Segoe UI", 12),
            relief='sunken', bd=3,
            selectbackground=self.ACCENT,
            selectforeground="#ffffff",
            activestyle='none',
            highlightthickness=0,
            yscrollcommand=scrollbar.set
        )
        self.results_list.pack(fill='both', expand=True)
        scrollbar.config(command=self.results_list.yview)

    #Placeholder helpers
    def _set_placeholder(self):
        self.query_input.insert('1.0', 'e.g.  Hillary AND Clinton')
        self.query_input.config(fg=self.MUTED)
        self.query_input.bind('<FocusIn>',  self._clear_placeholder)
        self.query_input.bind('<FocusOut>', self._add_placeholder)

    def _clear_placeholder(self, e):
        if self.query_input.get('1.0','end-1c') == 'e.g.  Hillary AND Clinton':
            self.query_input.delete('1.0','end')
            self.query_input.config(fg=self.TEXT)

    def _add_placeholder(self, e):
        if not self.query_input.get('1.0','end-1c').strip():
            self.query_input.insert('1.0','e.g.  Hillary AND Clinton')
            self.query_input.config(fg=self.MUTED)

    def _insert(self, text):
        self._clear_placeholder(None)
        self.query_input.insert('insert', text)
        self.query_input.focus()

    #Query runner
    def _run_query(self):
        q = self.query_input.get('1.0', 'end-1c').strip()
        if not q or q == 'e.g.  Hillary AND Clinton': return

        try:
            ids = process_query(q, self.index, self.positional, self.docmap)
        except Exception as e:
            self.stat_count.config(text="ERR", fg=self.FAIL)
            self.stat_query.config(text=str(e))
            self.results_list.delete(0, 'end')
            return

        # Update stats bar
        self.stat_count.config(text=str(len(ids)),
                               fg=self.PASS if ids else self.FAIL)
        self.stat_query.config(text=f"Query: {q}", fg=self.MUTED)

        # Populate results list
        self.results_list.delete(0, 'end')
        if not ids:
            self.results_list.insert('end', "  😔 No documents found for this query.")
            self.results_list.itemconfig(0, fg=self.MUTED)
        else:
            for doc_id in ids:
                filename = self.docmap.get(str(doc_id), '—')
                self.results_list.insert('end', f"  📄 [{str(doc_id).zfill(3)}]  {filename}")

        # Update history
        self._add_history(q, len(ids))

    def _add_history(self, q, count):
        self.history = [h for h in self.history if h[0] != q]
        self.history.insert(0, (q, count))
        if len(self.history) > 6: self.history.pop()

        for w in self.hist_frame.winfo_children(): w.destroy()
        for hq, hc in self.history:
            row = tk.Frame(self.hist_frame, bg=self.CARD, cursor='hand2')
            row.pack(fill='x', pady=2)
            tk.Label(row, text="🔸", bg=self.CARD, fg=self.HIGHLIGHT,
                     font=("Segoe UI", 8)).pack(side='left', padx=(6,4), pady=4)
            tk.Label(row, text=hq, bg=self.CARD, fg=self.TEXT,
                     font=("Segoe UI", 10), anchor='w').pack(side='left', fill='x', expand=True)
            tk.Label(row, text=f"📄 {hc} docs", bg=self.CARD, fg=self.SECONDARY,
                     font=("Segoe UI", 9, "bold")).pack(side='right', padx=8)
            row.bind('<Button-1>', lambda e, q=hq: self._load_history(q))
            for child in row.winfo_children():
                child.bind('<Button-1>', lambda e, q=hq: self._load_history(q))

    def _load_history(self, q):
        self.query_input.delete('1.0', 'end')
        self.query_input.config(fg=self.TEXT)
        self.query_input.insert('1.0', q)
        self._run_query()

root = tk.Tk()
app = IRApp(root)
root.mainloop()
