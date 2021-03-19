#include <stdio.h>
#include <vector>

#include "buffer.h"

class CTableFormater
{
public:
	enum {
		FORMAT_ALIGNED,
		FORMAT_TABBED,
		FORMAT_COMMA,
	};
	CTableFormater();
	~CTableFormater();
	void NewRow(void);
	void ClearRow(void);
	void Cell(const char *fmt,...) __attribute__((__format__(printf,2,3)));
	void CellV(const char *fmt,...) __attribute__((__format__(printf,2,3)));
	void Dump(FILE *, int fmt=FORMAT_ALIGNED);

private:
	struct cell_t {
		unsigned int off;
		unsigned int len;
		cell_t():off(0),len(0){}
		~cell_t() { }
	};
	typedef std::vector<cell_t> row_t;

	class buffer buf;
	std::vector<row_t> table;
	std::vector<unsigned int> width;
};

