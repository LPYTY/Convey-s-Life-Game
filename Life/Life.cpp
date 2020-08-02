#include "framework.h"
#include <cmath>
#include "life.h"
#include <thread>

#define RNDRATE 1

/*
	0------------------>i
	|
	|
	|
	|
	|
	|
	j
*/

using namespace std;

// Globals
int side_length = 10;
unsigned int xpivot = 0x08000000, ypivot = 0x08000000;
bool needErase, started;
unsigned int selected_builtin, selected_direction, kbd_input_state, TIMER = 500;
unsigned __int3264 active_timer;
const unsigned int move_length = 30;
HWND hwnd, hdialog;
char ids_help_about[256], ids_help_help[1024];

inline void redraw_erase() {
	RECT rect;
	GetClientRect(hwnd, &rect);
	needErase = true;
	RedrawWindow(hwnd, &rect, 0, RDW_ERASE | RDW_INVALIDATE);
}

inline void change_xpivot() {
	char xpivot_c[16];
	sprintf_s(xpivot_c, "0x%08x", xpivot);
	HWND hXPivot = GetDlgItem(hdialog, IDC_XPIVOT);
	SendMessage(hXPivot, WM_SETTEXT, 0, (LPARAM)xpivot_c);
}

inline void change_ypivot() {
	char ypivot_c[16];
	sprintf_s(ypivot_c, "0x%08x", ypivot);
	HWND hYPivot = GetDlgItem(hdialog, IDC_YPIVOT);
	SendMessage(hYPivot, WM_SETTEXT, 0, (LPARAM)ypivot_c);
}

class Map {

public:
	Map() {
		head_pool = new head[SIZE];
		node_pool = new node[SIZE];
		memset(head_pool, 0, SIZE * sizeof(head));
		memset(node_pool, 0, SIZE * sizeof(node));
		for (register int i = 0; i < SIZE; i++) (head_pool + i)->pnext = head_pool + i + 1;
		for (register int i = 0; i < SIZE; i++) (node_pool + i)->pnext = node_pool + i + 1;
		(head_pool + SIZE - 1)->pnext = nullptr;  (node_pool + SIZE - 1)->pnext = nullptr;
		cur.pnext = nullptr;
		phead_pools = { head_pool, nullptr, nullptr };
		pnode_pools = { nullptr, node_pool, nullptr };
		init_builtins();
	}

	void change(unsigned int xpos, unsigned int ypos, int type = 0) { //type: {0: 1->0, 0->1; 1: 0,1->1; 2: 0,1->0}
		register head* px = &cur;
		while (px->pnext && px->pnext->x <= xpos) px = px->pnext;
		if (px->x == xpos && px->pnode) {
			register node* py = px->pnode;
			while (py->pnext && py->pnext->y < ypos) py = py->pnext;
			if (py->pnext && py->pnext->y == ypos) { if (type != 1) del(py); }	//If the node already exists: destroy the node
			else if(type != 2) {										//If the node doesn't exist: insert a node
				node* pn = insert(py);
				pn->y = ypos;
				pn->state = true;
			}
		}
		else if (type != 2) {											//If the row doesn't exist: insert head and node
			head* pn = insert(px);
			node* pnode = insert(pn->pnode);

			pn->x = xpos;
			pnode->y = ypos;
			pnode->state = true;
		}
	}

	void calc() {
		register head* px = cur.pnext;
		while (px) {
			register node* py = px->pnode->pnext;
			for (; py; py = py->pnext) {
				if (!py->state) continue;
				int x = px->x;
				int y = py->y;
				for (register int i = x - 1; i <= x + 1; i++)
					for (register int j = y - 1; j <= y + 1; j++) add(i, j);
				mark(x, y);
			}
			px = px->pnext;
		}
		clear(&pre);
		pre.pnext = cur.pnext;
		cur.pnext = nullptr;
		px = nxt.pnext;
		while (px) {
			register node* py = px->pnode->pnext;
			for (; py; py = py->pnext) {
				if (py->count == 3) change(px->x, py->y);
				else if (py->state && py->count == 4) change(px->x, py->y);
			}
			px = px->pnext;
		}
		clear(&nxt);
	}

	void clear() {
		Sleep(TIMER);
		memset(head_pool, 0, SIZE * sizeof(head));
		memset(node_pool, 0, SIZE * sizeof(node));
		for (register int i = 0; i < SIZE; i++) (head_pool + i)->pnext = head_pool + i + 1;
		for (register int i = 0; i < SIZE; i++) (node_pool + i)->pnext = node_pool + i + 1;
		(head_pool + SIZE - 1)->pnext = nullptr;  (node_pool + SIZE - 1)->pnext = nullptr;
		cur.pnext = nullptr, nxt.pnext = nullptr, pre.pnext = nullptr, ppre = nullptr;
		redraw_erase();
		
		ppool* phead = phead_pools.pnext, *pdel = phead;
		while (phead != nullptr) {
			delete[] phead->phead;
			phead = phead->pnext;
			delete pdel;
			pdel = phead;
		}
		phead_pools.pnext = nullptr;

		ppool* pnode = pnode_pools.pnext; pdel = pnode;
		while (pnode != nullptr) {
			delete[] pnode->pnode;
			pnode = pnode->pnext;
			delete pdel;
			pdel = pnode;
		}
		pnode_pools.pnext = nullptr;

		headpool_usage = nodepool_usage = 1;
	}

	void add_builtin(const unsigned int& xpos, const unsigned int& ypos, const unsigned int& b = selected_builtin, const unsigned int& d = selected_direction) {
		if (b >= 10 || d >= 8) return;
		register unsigned int s = builtins[b].size, l = builtins[b].length - 1, h = builtins[b].height - 1;
		const POINT* cur = builtins[b].points;
		switch (d) {
		case 0:
			for (register unsigned int i = 0; i < s; i++) change(xpos + cur[i].x, ypos + cur[i].y, 1);
			break;
		case 1:
			for (register unsigned int i = 0; i < s; i++) change(xpos + cur[i].x, ypos + h - cur[i].y, 1);
			break;
		case 2:
			for (register unsigned int i = 0; i < s; i++) change(xpos + l - cur[i].x, ypos + cur[i].y, 1);
			break;
		case 3:
			for (register unsigned int i = 0; i < s; i++) change(xpos + l - cur[i].x, ypos + h - cur[i].y, 1);
			break;
		case 4:
			for (register unsigned int i = 0; i < s; i++) change(xpos + cur[i].y, ypos + cur[i].x, 1);
			break;
		case 5:
			for (register unsigned int i = 0; i < s; i++) change(xpos + cur[i].y, ypos + l - cur[i].x, 1);
			break;
		case 6:
			for (register unsigned int i = 0; i < s; i++) change(xpos + h - cur[i].y, ypos + cur[i].x, 1);
			break;
		case 7:
			for (register unsigned int i = 0; i < s; i++) change(xpos + h - cur[i].y, ypos + l - cur[i].x, 1);
			break;
		}
	}

	void draw(HDC hdc, RECT rect, bool erase = needErase, HBRUSH hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH), HBRUSH hWhiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH)) {
		needErase = false;
		unsigned int left = rect.left / side_length + xpivot, right = rect.right / side_length + xpivot;
		unsigned int top = rect.top / side_length + ypivot, bottom = rect.bottom / side_length + ypivot;
		register head* pl = &cur, *ppl = &pre;
		while (pl->pnext && pl->pnext->x < left) pl = pl->pnext;
		while (ppl->pnext && ppl->pnext->x < left) ppl = ppl->pnext;
		register head* px = pl, *ppx = ppl;
		while (px->pnext && px->pnext->x <= right) {
			if (!erase) while (ppx->pnext && ppx->pnext->x < px->pnext->x) ppx = ppx->pnext;
			register node* py = px->pnext->pnode, *ppy = (ppx->pnext && ppx->pnext->x == px->pnext->x) ? ppx->pnext->pnode : nullptr;
			while (py->pnext && py->pnext->y < top) py = py->pnext;
			if (!erase && ppy) while (ppy->pnext && ppy->pnext->y < top) ppy = ppy->pnext;
			while (py->pnext && py->pnext->y <= bottom) {
				if (ppy && !erase) {
					while (ppy->pnext && ppy->pnext->y < py->pnext->y) ppy = ppy->pnext;
					if (ppy->pnext && ppy->pnext->y == py->pnext->y) { py = py->pnext; continue; }
				}
				RECT fill_rect = {
					(LONG)((px->pnext->x - xpivot) * side_length + 1), //left
					(LONG)((py->pnext->y - ypivot) * side_length + 1), //top
					(LONG)((px->pnext->x - xpivot + 1) * side_length), //right
					(LONG)((py->pnext->y - ypivot + 1) * side_length)  //bottom
				};
				FillRect(hdc, &fill_rect, hBlackBrush);
				py = py->pnext;
			}
			px = px->pnext;
		}
		if (erase) return;
		px = pl, ppx = ppl;
		while (ppx->pnext && ppx->pnext->x <= right) {
			while (px->pnext && px->pnext->x < ppx->pnext->x) px = px->pnext;
			register node* ppy = ppx->pnext->pnode, *py = (px->pnext && px->pnext->x == ppx->pnext->x) ? px->pnext->pnode : nullptr;
			while (ppy->pnext && ppy->pnext->y < top) ppy = ppy->pnext;
			if (py) while (py->pnext && py->pnext->y < top) py = py->pnext;
			while (ppy->pnext && ppy->pnext->y <= bottom) {
				if (py) {
					while (py->pnext && py->pnext->y < ppy->pnext->y) py = py->pnext;
					if (py->pnext && py->pnext->y == ppy->pnext->y) { ppy = ppy->pnext; continue; }
				}
				RECT fill_rect = {
					(LONG)((ppx->pnext->x - xpivot) * side_length + 1), //left
					(LONG)((ppy->pnext->y - ypivot) * side_length + 1), //top
					(LONG)((ppx->pnext->x - xpivot + 1) * side_length), //right
					(LONG)((ppy->pnext->y - ypivot + 1) * side_length)  //bottom
				};
				FillRect(hdc, &fill_rect, hWhiteBrush);
				ppy = ppy->pnext;
			}
			ppx = ppx->pnext;
		}
	}

	inline RECT get_builtin_info(int b = selected_builtin) {
		if (b < 0 || b > 10) return { 0, 0, 0, 0 };
		return { 0, 0, builtins[b].length, builtins[b].height };
	}

	void draw_builtin(const unsigned int& b = selected_builtin, const unsigned int& d = selected_direction) {
		//Init
		const int xl = 71, yl = 58;
		HWND hPreview = GetDlgItem(hdialog, IDC_PREVIEW);
		if (!hPreview) return;
		const static HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
		const static HBRUSH white_brush = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
		const static HBRUSH red_brush = CreateSolidBrush(RGB(255, 0, 0));
		const builtin& bi = builtins[b];
		HDC hdc = GetDC(hPreview);
		
		//Clear
		RECT erase_rect = { 1, 1, xl, yl };
		FillRect(hdc, &erase_rect, white_brush);
		if (b >= 10 || d >= 8 || !bi.size) return;
		
		//Calc
		int length = d & 4 ? bi.height : bi.length, height = (d & 4 ? bi.length : bi.height);
		int xr = xl / length, yr = yl / height, r = xr < yr ? xr : yr;
		if (!r) return;
		int xs = ((xl - r * length) >> 1) + 1, ys = ((yl - r * height) >> 1) + 1;
		
		//Draw
		for (int i = 0; i < bi.size; i++) {
			int x = bi.points[i].x, y = bi.points[i].y;
			int l = length - 1, h = height - 1;
			if (d & 4) x = y, y = bi.points[i].x;
			switch (d) {
			case 0:
				break;
			case 2:
			case 6:
				x = l - x;
				break;
			case 1:
			case 5:
				y = h - y;
				break;
			case 3:
			case 7:
				x = l - x, y = h - y;
				break;
			}
			RECT rect = { x * r + xs, y * r + ys, (x + 1) * r + xs, (y + 1) * r + ys };
			FillRect(hdc, &rect, black_brush);
		}
		RECT click_rect = { xs, ys, xs + r, ys + r };
		FillRect(hdc, &click_rect, red_brush);
	}

	inline char* get_size() {
		static char size[5];
		_itoa_s(SIZE, size, 10);
		return size;
	}

private:
	struct node {
		unsigned int y;
		bool state;
		BYTE count;
		node* pnext;
	};

	struct head {
		unsigned int x;
		node* pnode;
		head* pnext;
	};

	struct builtin {
		const POINT* points;
		int size;
		int length;
		int height;
	};

	struct ppool {
		head* phead;
		node* pnode;
		ppool* pnext;
	};

	head* enlarge_head_pool() {
		head* nhead_pool = new head[SIZE];
		memset(nhead_pool, 0, SIZE * sizeof(head));
		for (register int i = 0; i < SIZE; i++) (nhead_pool + i)->pnext = nhead_pool + i + 1;
		(nhead_pool + SIZE - 1)->pnext = nullptr;
		head_pool->pnext = nhead_pool;
		
		ppool* nphead_pool = new ppool;
		nphead_pool->phead = nhead_pool;
		nphead_pool->pnext = phead_pools.pnext;
		phead_pools.pnext = nphead_pool;

		headpool_usage++;
		refresh_headpool_usage();

		return nhead_pool;
	}

	node* enlarge_node_pool() {
		node* nnode_pool = new node[SIZE];
		memset(nnode_pool, 0, SIZE * sizeof(node));
		for (register int i = 0; i < SIZE; i++) (nnode_pool + i)->pnext = nnode_pool + i + 1;
		(nnode_pool + SIZE - 1)->pnext = nullptr;
		node_pool->pnext = nnode_pool;

		ppool* npnode_pool = new ppool;
		npnode_pool->pnode = nnode_pool;
		npnode_pool->pnext = pnode_pools.pnext;
		pnode_pools.pnext = npnode_pool;

		nodepool_usage++;
		refresh_nodepool_usage();

		return nnode_pool;
	}

	inline void refresh_headpool_usage() {
		HWND hHeadpoolUsage = GetDlgItem(hdialog, IDC_HEADPOOL_USAGE);
		char headpool_usage_c[8] = "";
		_itoa_s(headpool_usage, headpool_usage_c, 10);
		SendMessage(hHeadpoolUsage, WM_SETTEXT, 0, (LPARAM)headpool_usage_c);
	}

	inline void refresh_nodepool_usage() {
		HWND hNodepoolUsage = GetDlgItem(hdialog, IDC_NODEPOOL_USAGE);
		char nodepool_usage_c[8] = "";
		_itoa_s(nodepool_usage, nodepool_usage_c, 10);
		SendMessage(hNodepoolUsage, WM_SETTEXT, 0, (LPARAM)nodepool_usage_c);
	}

	head* insert(head* p) {
		head* pn = head_pool->pnext ? head_pool->pnext : enlarge_head_pool();
		head_pool->pnext = pn->pnext;
		pn->pnext = p->pnext;
		p->pnext = pn;
		
		node* pnode = node_pool->pnext ? node_pool->pnext : enlarge_node_pool();
		node_pool->pnext = pnode->pnext;
		pn->pnode = pnode;
		pnode->pnext = nullptr;
		return pn;
	}

	node* insert(node* p) {
		node* pn = node_pool->pnext ? node_pool->pnext : enlarge_node_pool();
		node_pool->pnext = pn->pnext;
		pn->pnext = p->pnext;
		p->pnext = pn;
		return pn;
	}

	void del(node* p) {
		node* pd = p->pnext;
		p->pnext = pd->pnext;
		pd->pnext = node_pool->pnext;
		node_pool->pnext = pd;
		pd->count = pd->state = pd->y = 0;
	}

	void del(head* h) {
		head* pd = h->pnext;
		
		node* pdn = pd->pnode;
		pd->pnode = nullptr;
		pdn->pnext = node_pool->pnext;
		node_pool->pnext = pdn;
		pdn->count = pdn->state = pdn->y = 0;

		h->pnext = pd->pnext;
		pd->pnext = head_pool->pnext;
		head_pool->pnext = pd;
		pd->x = 0;
	}

	void add(unsigned int xpos, unsigned int ypos) {
		if (!ppre) ppre = &nxt;
		register head* px = &nxt;
		if (ppre->pnext && ppre->pnext->x <= xpos) px = ppre;
		while (px->pnext && px->pnext->x <= xpos) px = px->pnext;
		if (px->x == xpos && px->pnode) {
			register node* py = px->pnode;
			while (py->pnext && py->pnext->y <= ypos) py = py->pnext;
			if (py->y == ypos) py->count++;				//If the node already exists: add 1 to count
			else {										//If the node doesn't exist: insert a node
				node* pn = insert(py);
				pn->y = ypos;
				pn->count = 1;
			}
		}
		else {											//If the row doesn't exist: insert head and node
			head* pn = insert(px);
			node* pnode = insert(pn->pnode);

			pn->x = xpos;
			pnode->y = ypos;
			pnode->count = 1;
		}
		ppre = px;
	}

	void mark(unsigned int xpos, unsigned int ypos) {
		register head* px = &nxt;
		while (px->pnext && px->pnext->x <= xpos) px = px->pnext;
		register node* py = px->pnode;
		while (py->pnext && py->pnext->y <= ypos) py = py->pnext;
		py->state = true;
	}

	void clear(register head* h) {
		while (h->pnext) {
			node* hn = h->pnext->pnode;
			while (hn->pnext) del(hn);
			del(h);
		}
	}

	void init_builtins() {

		/*	{1,1,1},
			{1,0,0},
			{0,1,0}	*/

		const static POINT builtin0[] = {
			{0, 0}, {0, 1}, {1, 0}, {1, 2}, {2, 0}
		};
		builtins[0].points = builtin0;
		builtins[0].size = 5;
		builtins[0].length = 3;
		builtins[0].height = 3;

		/*	{0,1,0,0,1},
			{1,0,0,0,0},
			{1,0,0,0,1},
			{1,1,1,1,0}	*/
		
		const static POINT builtin1[] = {
			{0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 3}, {2, 3}, {3, 3}, {4, 0}, {4, 2}
		};
		builtins[1].points = builtin1;
		builtins[1].size = 9;
		builtins[1].length = 5;
		builtins[1].height = 4;

		/*	
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
			{0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
			{1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
			{1,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,1,1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}	*/

		const static POINT builtin2[] = {
			{0, 4}, {0, 5}, {1, 4}, {1, 5},
			{10, 4}, {10, 5}, {10, 6}, {11, 3}, {11, 7}, {12, 2}, {12, 8}, {13, 2}, {13, 8},
			{14, 5}, {15, 3}, {15, 7}, {16, 4}, {16, 5}, {16, 6}, {17, 5},
			{20, 2}, {20, 3}, {20, 4}, {21, 2}, {21, 3}, {21, 4}, {22, 1}, {22, 5},
			{24, 0}, {24, 1}, {24, 5}, {24, 6},
			{34, 2}, {34, 3}, {35, 2}, {35, 3}
		};
		builtins[2].points = builtin2;
		builtins[2].size = 36;
		builtins[2].length = 36;
		builtins[2].height = 9;

		/*
			{0,0,0,0,0,0,1,0,0,0,0,0,0},
			{0,0,0,0,0,1,0,1,0,0,0,0,0},
			{0,0,0,0,0,1,0,1,0,0,0,0,0},
			{0,0,0,0,0,0,1,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0,0},
			{0,1,1,0,0,0,0,0,0,0,1,1,0},
			{1,0,0,1,0,0,0,0,0,1,0,0,1},
			{0,1,1,0,0,0,0,0,0,0,1,1,0},
			{0,0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,1,0,0,0,0,0,0},
			{0,0,0,0,0,1,0,1,0,0,0,0,0},
			{0,0,0,0,0,1,0,1,0,0,0,0,0},
			{0,0,0,0,0,0,1,0,0,0,0,0,0}
		*/

		const static POINT builtin3[] = {
			{0, 6}, {1, 5}, {1, 7}, {2, 5}, {2, 7}, {3, 6},
			{5, 1}, {5, 2}, {6, 0}, {6, 3}, {7, 1}, {7, 2},
			{5, 10}, {5, 11}, {6, 9}, {6, 12}, {7, 10}, {7, 11},
			{9, 6}, {10, 5}, {10, 7}, {11, 5}, {11, 7}, {12, 6}
		};
		builtins[3].points = builtin3;
		builtins[3].size = 24;
		builtins[3].length = 13;
		builtins[3].height = 13;
	}

	head pre;	//Previous map
	head cur;	//Current map
	head nxt;	//Next map
	head* ppre;
	head* head_pool;
	node* node_pool;
	int headpool_usage = 1, nodepool_usage = 1;
	builtin builtins[10];
	const int SIZE = 50;
	ppool phead_pools, pnode_pools;
};

Map map;

inline void tst() {
	//map.change(1, 1);
	//map.change(2, 1);
	//map.enlarge();
}

void onPaint(HWND hWnd) {
	PAINTSTRUCT paintStruct;
	HDC hdc = BeginPaint(hWnd, &paintStruct);
	RECT updateRect = paintStruct.rcPaint;

	static HBRUSH hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH); //CreateSolidBrush(RGB(0x00, 0x00, 0x00));

	static HPEN hLinePen = CreatePen(PS_SOLID, 1, RGB(0xaa, 0xaa, 0xaa));
	SelectObject(hdc, hBlackBrush);
	SelectObject(hdc, hLinePen);

	/*  lines  */
	if (needErase) {
		for (register int i = updateRect.left / side_length; i <= updateRect.right / side_length; i++) {
			MoveToEx(hdc, i * side_length, updateRect.top, nullptr);
			LineTo(hdc, i * side_length, updateRect.bottom);
		}
		for (register int i = updateRect.top / side_length; i <= updateRect.bottom / side_length; i++) {
			MoveToEx(hdc, updateRect.left, i * side_length, nullptr);
			LineTo(hdc, updateRect.right, i * side_length);
		}
	}

	/*  blocks  */
	map.draw(hdc, updateRect);

	EndPaint(hWnd, &paintStruct);
	return;
}

DWORD WINAPI timer(LPVOID c) {
	unsigned __int3264 current_timer = (unsigned __int3264)c;
	while (true) {
		do Sleep(TIMER); while (!started && current_timer == active_timer);
		if (current_timer != active_timer) return 0;
		RECT rect;
		GetClientRect(hwnd, &rect);
		map.calc();
		RedrawWindow(hwnd, &rect, 0, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

/* This is where all the input to the window goes to */
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {

		/* Upon destruction, tell the main thread to stop */
	case WM_DESTROY: {
		PostQuitMessage(0);
		break;
	}

	case WM_PAINT: {
		onPaint(hwnd);
		break;
	}

	case WM_SIZE: {
		redraw_erase();
		return DefWindowProc(hwnd, Message, wParam, lParam);
		break;
	}

	case WM_LBUTTONDOWN: {
		POINTS cpos = MAKEPOINTS(lParam);
		int xc = cpos.x / side_length, yc = cpos.y / side_length;
		map.change(xc + xpivot, yc + ypivot);
		RECT crect = { xc * side_length + 1, yc * side_length + 1, (xc + 1) * side_length, (yc + 1) * side_length };
		needErase = true;
		RedrawWindow(hwnd, &crect, 0, RDW_INVALIDATE | RDW_ERASE);
		return DefWindowProc(hwnd, Message, wParam, lParam);
		break;
	}

	case WM_KEYDOWN: {
		switch (wParam) {
		case VK_LEFT: {
			xpivot -= move_length / side_length;
			redraw_erase();
			change_xpivot();
			break;
		}
		case VK_RIGHT: {
			xpivot += move_length / side_length;
			redraw_erase();
			change_xpivot();
			break;
		}
		case VK_UP: {
			ypivot -= move_length / side_length;
			redraw_erase();
			change_ypivot();
			break;
		}
		case VK_DOWN: {
			ypivot += move_length / side_length;
			redraw_erase();
			change_ypivot();
			break;
		}
		case VK_SPACE: {
			started = !started;
			break;
		}
		case 'B': {
			kbd_input_state = kbd_input_state != 1 ? 1 : 0;
			break;
		}
		case 'D': {
			kbd_input_state = kbd_input_state != 2 ? 2 : 0;
			break;
		}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			int idc;
			if (kbd_input_state) idc = kbd_input_state == 1 ? IDC_BUILTIN : IDC_DIRECTION;
			else break;
			HWND h = GetDlgItem(hdialog, idc);
			char buf[2] = "";
			_itoa_s((int)(wParam) - '0', buf, 2, 10);
			SendMessage(h, WM_SETTEXT, 0, (LPARAM)buf);
			break;
		}
		default: {
			break;
		}
		}
		return DefWindowProc(hwnd, Message, wParam, lParam);
		break;
	}

	case WM_RBUTTONDOWN: {
		POINTS cpos = MAKEPOINTS(lParam);
		int xc = cpos.x / side_length, yc = cpos.y / side_length;
		map.add_builtin(xc + xpivot, yc + ypivot);
		RECT crect;
		if (selected_direction < 0);
		else if (selected_direction < 4)
			crect = { xc * side_length + 1, yc * side_length + 1, (xc + map.get_builtin_info().right) * side_length, (yc + map.get_builtin_info().bottom) * side_length };
		else if (selected_direction < 8)
			crect = { xc * side_length + 1, yc * side_length + 1, (xc + map.get_builtin_info().bottom) * side_length, (yc + map.get_builtin_info().right) * side_length };
		needErase = true;
		RedrawWindow(hwnd, &crect, 0, RDW_INVALIDATE | RDW_ERASE); 
		return DefWindowProc(hwnd, Message, wParam, lParam);
		break;
	}

	case WM_MOUSEWHEEL: {
		HWND hScale = GetDlgItem(hdialog, IDC_SCALE);
		char str[16];
		GetWindowText(hScale, str, 16);
		int i = atoi(str) + (wParam & 0x80000000 ? -2 : 2);
		if (i < 3) i = 3;
		_itoa_s(i, str, 10);
		SetWindowText(hScale, str);
		return DefWindowProc(hwnd, Message, wParam, lParam);
		break;
	}

	case WM_COMMAND: {
		switch (wParam) {
		case ID_HELP_HELP: {
			MessageBox(hwnd, ids_help_help, "Help", MB_OK);
			break;
		}
		case ID_HELP_ABOUT: {
			MessageBox(hwnd, ids_help_about, "About", MB_OK);
			break;
		}
		}
		break;
	}
					 /* All other messages (a lot of them) are processed using default procedures */
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK DlgProc(HWND hdialog, UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {
	case WM_INITDIALOG: {
		HWND hBuiltin = GetDlgItem(hdialog, IDC_BUILTIN);
		SendMessage(hBuiltin, EM_SETLIMITTEXT, 1, 0);
		SendMessage(hBuiltin, WM_SETTEXT, 0, (LPARAM)"0");

		HWND hDirection = GetDlgItem(hdialog, IDC_DIRECTION);
		SendMessage(hDirection, EM_SETLIMITTEXT, 1, 0);
		SendMessage(hDirection, WM_SETTEXT, 0, (LPARAM)"0");

		HWND hTimer = GetDlgItem(hdialog, IDC_TIMER);
		SendMessage(hTimer, EM_SETLIMITTEXT, 5, 0);
		SendMessage(hTimer, WM_SETTEXT, 0, (LPARAM)"50");

		HWND hScale = GetDlgItem(hdialog, IDC_SCALE);
		SendMessage(hScale, EM_SETLIMITTEXT, 3, 0);
		SendMessage(hScale, WM_SETTEXT, 0, (LPARAM)"10");

		HWND hHeadpoolSize = GetDlgItem(hdialog, IDC_HEADPOOL_SIZE);
		SendMessage(hHeadpoolSize, WM_SETTEXT, 0, (LPARAM)map.get_size());

		HWND hNodepoolSize = GetDlgItem(hdialog, IDC_NODEPOOL_SIZE);
		SendMessage(hNodepoolSize, WM_SETTEXT, 0, (LPARAM)map.get_size());

		return true;
	}

	case WM_COMMAND: {
		switch (wParam)
		{
		case (EN_CHANGE << 16) + IDC_BUILTIN: {
			HWND hBuiltin = (HWND)lParam;
			char val[16];
			GetWindowText(hBuiltin, val, 16);
			selected_builtin = atoi(val);
			map.draw_builtin();
			break;
		}

		case (EN_CHANGE << 16) + IDC_DIRECTION: {
			HWND hDire = (HWND)lParam;
			char val[16];
			GetWindowText(hDire, val, 16);
			selected_direction = atoi(val);
			map.draw_builtin();
			break;
		}

		case (EN_CHANGE << 16) + IDC_TIMER: {
			HWND hTimer = (HWND)lParam;
			char val[16];
			GetWindowText(hTimer, val, 16);
			int v = atoi(val);
			if (v <= 0) SendMessage(hTimer, WM_SETTEXT, 0, (LPARAM)"1");
			else {
				CloseHandle(CreateThread(nullptr, 0, timer, (LPVOID)++active_timer, 0, nullptr));
				TIMER = v;
			}
			break;
		}

		case (EN_CHANGE << 16) + IDC_SCALE: {
			HWND hScale = (HWND)lParam;
			char val[16];
			GetWindowText(hScale, val, 16);
			int v = atoi(val);
			if (v <= 2) SendMessage(hScale, WM_SETTEXT, 0, (LPARAM)"3");
			else {
				side_length = v;
				redraw_erase();
			}
			break;
		}

		case IDC_STARTSTOP: {
			started = !started;
			break;
		}
							
		case IDC_RESET: {
			xpivot = ypivot = 0x08000000;
			started = false;
			map.clear();
			break;
		}

		default:
			break;
		}
		break;
	}
	default: {
		break;
	}
	}
	return false;
}

/* The 'main' function of Win32 GUI programs: this is where execution starts */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	LoadString(hInstance, IDS_HELP_ABOUT, ids_help_about, 256);
	LoadString(hInstance, IDS_HELP_HELP, ids_help_help, 1024);

	WNDCLASSEX wc; /* A properties struct of our window */
	//HWND hwnd; /* A 'HANDLE', hence the H, or a pointer to our window */
	MSG msg; /* A temporary location for all messages */

	/* zero out the struct and set the stuff we want to modify */
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc; /* This is where we will send messages to */
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	/* White, COLOR_WINDOW is just a #define for a system color, try Ctrl+Clicking it */
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = "WindowClass";
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); /* Load a standard icon */
	wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION); /* use the name "A" to use the project icon */
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);

	if (!RegisterClassEx(&wc)) {
		MessageBox(nullptr, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	needErase = true;
	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "WindowClass", "Life", WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, /* x */
		CW_USEDEFAULT, /* y */
		640, /* width */
		480, /* height */
		nullptr, nullptr, hInstance, nullptr);

	if (hwnd == nullptr) {
		MessageBox(nullptr, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hdialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), hwnd, (DLGPROC)DlgProc);
	ShowWindow(hdialog, SW_NORMAL);

	/*	Initializations	*/
	srand(GetTickCount());

	tst();

	CloseHandle(CreateThread(nullptr, 0, timer, (LPVOID)++active_timer, 0, nullptr));

	/*
		This is the heart of our program where all input is processed and
		sent to WndProc. Note that GetMessage blocks code flow until it receives something, so
		this loop will not produce unreasonably high CPU usage
	*/
	while (GetMessage(&msg, nullptr, 0, 0) > 0) { /* If no error is received... */
		TranslateMessage(&msg); /* Translate key codes to chars if present */
		DispatchMessage(&msg); /* Send it to WndProc */
	}
	return (int)msg.wParam;
}
