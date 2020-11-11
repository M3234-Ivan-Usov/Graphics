#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<cstdlib>
#include<string>
#include<vector>
#include<chrono>
#include<initializer_list>
#include<algorithm>

typedef unsigned char byte;

using std::string;
using std::vector;
using std::find;


struct file;
struct components;
class IllegalArgumentException;
class args;
class Base;
class Simple;
class YCbCr;
class HSX;

args parse(int argc, char** argv);
file* parse(FILE* stream);
file* parse(FILE* f1, FILE* f2, FILE* f3);
bool isSameFile(file* in, const char* f, int w, int h, int m);

components* transformToRGB(args& input);
components* transformFromRGB(args& input, components* RGB);
components* move(Base&& obj);
void clean(components* data);

struct file {
	char format[3] = "P6";
	int width = 0, height = 0, size = 0;
	int maxcolour = 0;
	bool separated = false;
	byte* bytes = nullptr;
};

struct components {
	int size;
	// Red | Cyan | Hue | Luma
	double* x;
	// Green | Magenta | Chrominance orange | Saturation | Blue-difference
	double* y;
	//Blue | Yellow | Chrominance green | Lightness, Value | Red-difference
	double* z;
};

class IllegalArgumentException {
public:

	IllegalArgumentException(string message) {
		this->message = message;
	}
	void what() {
		std::cerr << message;
	}

private:
	string message;
};

class args {
public:

	int fromColourSpace;
	int toColourSpace;

	vector<FILE*> files;

	file* meta;

	args() : fromColourSpace(-1), toColourSpace(-1), meta(new file) {}
	args(args&& move) : files(move.files), meta(new file) {
		fromColourSpace = move.fromColourSpace;
		toColourSpace = move.toColourSpace;
		strcpy(meta->format, move.meta->format);
		meta->width = move.meta->width;
		meta->height = move.meta->height;
		meta->separated = move.meta->separated;
		meta->size = move.meta->size;
		meta->maxcolour = move.meta->maxcolour;
		meta->bytes = move.meta->bytes;
		move.meta->bytes = nullptr;
	}

	~args() {
		delete[] meta->bytes;
		delete meta;
	}

	void operator<<(components* result) {
		if ((files.size() == 3) || (files.size() == 6)) {
			fill(files[0], result->x, result->size, 0);
			fill(files[1], result->y, result->size, 1);
			fill(files[2], result->z, result->size, 2);
		}
		else if ((files.size() == 1) || (files.size() == 4)) {
			fill(files[0], result);
		}
		else throw 1;
		clean(result);
	}

private:
	void fill(FILE* stream, components* result) {
		header(stream, "P6");
		for (int i = 0; i < 3 * result->size; i++) {
			switch (i % 3) {
			case 0:
				meta->bytes[i] = normalise(result->x[i / 3]);
				break;
			case 1:
				meta->bytes[i] = normalise(result->y[i / 3]);
				break;
			case 2:
				meta->bytes[i] = normalise(result->z[i / 3]);
				break;
			}
		}
		fwrite(meta->bytes, sizeof(byte), 3 * result->size, stream);
		fclose(stream);
	}
	void fill(FILE* stream, double* component, int size, int position) {
		header(stream, "P5");
		for (int i = 0; i < size; i++) {
			meta->bytes[size * position + i] = normalise(component[i]);
		}
		fwrite(meta->bytes + size * position, sizeof(byte), size, stream);
		fclose(stream);
	}
	void header(FILE* stream, const char * format) {
		fprintf(stream, "%s\n%d %d\n%d\n", format, meta->width, meta->height, meta->maxcolour);
	}

	byte normalise(double pixel) {
		if (pixel > 255) {
			return 255;
		}
		if (pixel < 0) {
			return 0;
		}
		return static_cast<byte>(pixel);
	}
};

class Base {
public:

	components* canals;

	Base(args & input) {
		try {
			canals = new components;
			canals->size = input.meta->size / 3;
			canals->x = new double[canals->size];
			canals->y = new double[canals->size];
			canals->z = new double[canals->size];
		}
		catch (std::bad_alloc e) {
			throw IllegalArgumentException("Failed to allocate separated components");
		}
		if (!input.meta->separated) {
			for (int i = 0; i < input.meta->size; i++) {
				switch (i % 3) {
				case 0:
					canals->x[i / 3] = input.meta->bytes[i];
					break;
				case 1:
					canals->y[i / 3] = input.meta->bytes[i];
					break;
				case 2:
					canals->z[i / 3] = input.meta->bytes[i];
					break;
				}
			}
		}
		else {
			for (int i = 0; i < canals->size; i++) {
				canals->x[i] = input.meta->bytes[i];
			}
			for (int i = 0; i < canals->size; i++) {
				canals->y[i] = input.meta->bytes[i + canals->size];
			}
			for (int i = 0; i < canals->size; i++) {
				canals->z[i] = input.meta->bytes[i + 2 * canals->size];
			}
		}
	}
	Base(components* move) {
		canals = new components;
		canals->size = move->size;
		canals->x = move->x;
		move->x = nullptr;
		canals->y = move->y;
		move->y = nullptr;
		canals->z = move->z;
		move->z = nullptr;
	}

	virtual ~Base() {
		clean(canals);
	}

protected:

	void matrixTransform(double matrix[12]) {
		double tempX, tempY, tempZ;
		for (int i = 0; i < canals->size; i++) {
			tempX = matrix[0] +
				matrix[1] * canals->x[i] +
				matrix[2] * canals->y[i] +
				matrix[3] * canals->z[i];
			tempY = matrix[4] +
				matrix[5] * canals->x[i] +
				matrix[6] * canals->y[i] +
				matrix[7] * canals->z[i];
			tempZ = matrix[8] +
				matrix[9] * canals->x[i] +
				matrix[10] * canals->y[i] +
				matrix[11] * canals->z[i];
			canals->x[i] = tempX;
			canals->y[i] = tempY;
			canals->z[i] = tempZ;
		}
	}

	double min(std::initializer_list<double> arg) {
		double min = *arg.begin();
		for (double x : arg)
			min = (min > x) ? x : min;
		return min;
	}

	double max(std::initializer_list<double> arg) {
		double max = *arg.begin();
		for (double x : arg)
			max = (max < x) ? x : max;
		return max;
	}

	double mod(double x, int y) {
		int f2 = static_cast<int>(x) / y;
		return x - static_cast<double>(f2) * static_cast<double>(y);
	}
};

class Simple : public Base {
public:

	Simple(args & input, double matrix[12], bool Y) : Base(input) {
		if (Y) preNormalise(true);
		unitNormalise();
		matrixTransform(matrix);
	}
	Simple(components* RGB, double matrix[12], bool Y) : Base(RGB) {
		matrixTransform(matrix);
		if (Y) preNormalise(false);
		fullNormalise();
	}

	//Standart 0..255 or 0..1 span
	void unitNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] /= 255.0;
			canals->y[i] /= 255.0;
			canals->z[i] /= 255.0;
		}
	}
	void fullNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] *= 255;
			canals->y[i] *= 255;
			canals->z[i] *= 255;
		}
	}
private:
	void preNormalise(bool toUnit) {
		if (toUnit) {
			for (int i = 0; i < canals->size; i++) {
				canals->y[i] -= 128;
				canals->z[i] -= 128;
			}
		}
		else {
			for (int i = 0; i < canals->size; i++) {
				canals->y[i] += 0.5;
				canals->z[i] += 0.5;
			}
		}
	}
};

class YCbCr : public Base {
public:

	double kr, kg, kb;

	YCbCr(args & input, double kr, double kg, double kb) : Base(input) {
		this->kr = kr;
		this->kg = kg;
		this->kb = kb;
		unitNormalise();
		double matrix[12] = {
			0, 1,              0,                    2 - 2 * kr,
			0, 1,   (kb / kg) * (2 * kb - 2),   (kr / kg) * (2 * kr - 2),
			0, 1,          2 - 2 * kb,                    0
		};
		matrixTransform(matrix);
	}
	YCbCr(components* RGB, double kr, double kg, double kb) : Base(RGB) {
		this->kr = kr;
		this->kg = kg;
		this->kb = kb;
		double matrix[12] = {
			0,        kr,                    kg,             kb,
			0,  kr / (2 * kb - 2), kg / (2 * kb - 2),        0.5,
			0,        0.5,         kg / (2 * kr - 2),  kb / (2 * kr - 2)
		};
		matrixTransform(matrix);
		fullNormalise();
	}

	//Digital(C): Y = 16..235, Cb =   16..240, Cr =   16..240
	//Analog (P): Y =  0..1,   Pb = -0.5..0.5, Pr = -0.5..0.5
	void unitNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] /= 255.0;
			canals->y[i] = (canals->y[i] - 128) / 255.0;
			canals->z[i] = (canals->z[i] - 128) / 255.0;
		}
	}
	void fullNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] *= 255;
			canals->y[i] = canals->y[i] * 255 + 128;
			canals->z[i] = canals->z[i] * 255 + 128;
		}
	}
};

class HSX : public Base {
public:

	HSX(args & input, bool HSV) : Base(input) {
		unitNormalise();
		double tempX, tempY, tempZ;
		for (int i = 0; i < canals->size; i++) {
			tempX = (HSV) ? HSVFunction(i, 5) : HSLFunction(i, 0);
			tempY = (HSV) ? HSVFunction(i, 3) : HSLFunction(i, 8);
			tempZ = (HSV) ? HSVFunction(i, 1) : HSLFunction(i, 4);
			canals->x[i] = tempX;
			canals->y[i] = tempY;
			canals->z[i] = tempZ;
		}

	}
	HSX(components* RGB, bool HSV) : Base(RGB) {
		double R, G, B, maximal, chroma;
		for (int i = 0; i < canals->size; i++) {
			R = canals->x[i], G = canals->y[i], B = canals->z[i];
			maximal = max({ R, G, B });
			chroma = maximal - min({ R, G, B });
			canals->x[i] = (chroma == 0) ? 0 :
				hue(R, G, B, chroma, maximal);
			canals->y[i] = saturation(HSV, chroma, maximal);
			canals->z[i] = (HSV) ? maximal : (maximal - chroma / 2);
		}
		fullNormalise();
	}

	//Native span is 0..360 for hue and 0..1 for others
	void unitNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] /= scale;
			canals->y[i] /= 255.0;
			canals->z[i] /= 255.0;
		}
	}
	void fullNormalise() {
		for (int i = 0; i < canals->size; i++) {
			canals->x[i] *= scale;
			canals->y[i] *= 255;
			canals->z[i] *= 255;
		}
	}
private:
	const double scale = 255.0 / 360.0;

	double HSVFunction(int i, int n) {
		double H = canals->x[i];
		double S = canals->y[i];
		double V = canals->z[i];
		double k = mod((n + H / 60.0), 6);
		double t = min({ k, 4.0 - k, 1.0 });
		return V - V * S * max({ 0.0, t });
	}
	double HSLFunction(int i, int n) {
		double H = canals->x[i];
		double S = canals->y[i];
		double L = canals->z[i];
		double k = mod((n + H / 30.0), 12);
		double a = S * min({ L, 1.0 - L });
		double t = min({ k - 3.0, 9.0 - k, 1.0 });
		return L - a * max({ -1.0, t });
	}

	double hue(double R, double G, double B, double chroma, double maximal) {
		double hue = 0;
		if (maximal == R)
			hue = (0 + (G - B) / chroma);
		if (maximal == G)
			hue = (2 + (B - R) / chroma);
		if (maximal == B)
			hue = (4 + (R - G) / chroma);
		return hue * 60;
	}
	double saturation(bool HSV, double chroma, double maximal) {
		if (HSV) {
			return (maximal == 0) ? 0 : (chroma / maximal);
		}
		else {
			double L = maximal - chroma / 2;
			bool zero = (L == 0) || (L == 1);
			return (zero) ? 0 : ((maximal - L) / min({ L, 1.0 - L }));
		}
	}
};

int main(int argc, char** argv)
{
	try {
		auto start = std::chrono::steady_clock::now();
		args input = parse(argc, argv);
		components* RGB = transformToRGB(input);
		components* result = transformFromRGB(input, RGB);
		input << result;
		auto end = std::chrono::steady_clock::now();
		std::cout << "finished in " << std::chrono::duration_cast
			<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
		return 0;
	}
	catch (IllegalArgumentException e) {
		e.what();
		return 1;
	}
}

args parse(int argc, char** argv)
{
	args input;
	const string colourSpaces[]{ "RGB", "CMY", "YCoCg", "YCbCr.601", "YCbCr.709", "HSL", "HSV", };
	vector<string> args;
	for (int i = 1; i < argc; i++) {
		args.push_back(argv[i]);
	}

	auto f = find(args.begin(), args.end(), "-f");
	auto t = find(args.begin(), args.end(), "-t");
	if ((f != args.end()) && (t != args.end())) {
		f++;
		t++;
		for (int i = 0; i < 7; i++) {
			if (colourSpaces[i] == *f) {
				input.fromColourSpace = i;
			}
			if (colourSpaces[i] == *t) {
				input.toColourSpace = i;
			}
		}
	}
	else
		throw IllegalArgumentException("-f and/or -t flags are failed to be found");

	auto i = find(args.begin(), args.end(), "-i");
	auto o = find(args.begin(), args.end(), "-o");
	if ((i != args.end()) && (o != args.end())) {
		i++, o++;
		if ((atoi((*i).c_str()) == 1) && (atoi((*o).c_str()) == 1)) {
			i++, o++;
			FILE* in = fopen((*i).c_str(), "rb");
			FILE* out = fopen((*o).c_str(), "wb");
			if ((in == NULL) || (out == NULL))
				throw IllegalArgumentException("Problems with file");
			input.files.push_back(out);
			input.meta = parse(in);
		}

		else if ((atoi((*i).c_str()) == 1) && (atoi((*o).c_str()) == 3)) {
			i++;
			FILE* in = fopen((*i).c_str(), "rb");
			if (in == NULL) 
				throw IllegalArgumentException("Problems with in file(s)");
			for (int j = 1; j <= 3; j++) {
				o++;
				FILE* out = fopen((*o).c_str(), "wb");
				if (out == NULL) 
					throw IllegalArgumentException("Problems with out file(s)");
				input.files.push_back(out);
			}
			input.meta = parse(in);
		}

		else if ((atoi((*i).c_str()) == 3) && (atoi((*o).c_str()) == 1)) {
			o++;
			FILE* out = fopen((*o).c_str(), "wb");
			if (out == NULL) 
				throw IllegalArgumentException("Problems with out file(s)");
			input.files.push_back(out);
			for (int j = 1; j <= 3; j++) {
				i++;
				FILE* in = fopen((*i).c_str(), "rb");
				if (in == NULL) 
					throw IllegalArgumentException("Problems with in file(s)");
				input.files.push_back(in);
			}
			input.meta = parse(input.files[1], input.files[2], input.files[3]);
		}

		else if ((atoi((*i).c_str()) == 3) && (atoi((*o).c_str()) == 3)) {
			for (int j = 1; j <= 3; j++) {
				++o;
				FILE* out = fopen((*o).c_str(), "wb");
				if (out == NULL)
					throw IllegalArgumentException("Problems with out file(s)");
				input.files.push_back(out);
			}
			for (int j = 1; j <= 3; j++) {
				++i;
				FILE* in = fopen((*i).c_str(), "rb");
				if (in == NULL) 
					throw IllegalArgumentException("Problems with in file(s)");
				input.files.push_back(in);
			}
			input.meta = parse(input.files[3], input.files[4], input.files[5]);
		}

		else {
			throw IllegalArgumentException("Incorrect -i and/or -o flags");
		}
	}
	return std::move(input);
}

file* parse(FILE* stream)
{
	file* in = new file;
	fscanf(stream, "%s\n%d %d\n%d\n", &in->format, &in->width, &in->height, &in->maxcolour);
	in->size = 3 * in->width * in->height;
	in->separated = false;
	try {
		in->bytes = new byte[in->size];
		fread(in->bytes, sizeof(byte), in->size, stream);
	}
	catch (std::bad_alloc) {
		throw IllegalArgumentException("Failed to allocate input data");
	};
	fclose(stream);
	return in;
}

file* parse(FILE* f1, FILE* f2, FILE* f3) {
	file* in = new file;
	char f[3] = "P5";
	int w, h, m;
	fscanf(f1, "%s\n%d %d\n%d\n", &in->format, &in->width, &in->height, &in->maxcolour);
	if (strcmp(in->format, "P5")) 
		throw IllegalArgumentException("Not a .pgm");
	fscanf(f2, "%s\n%d %d\n%d\n", f, &w, &h, &m);
	if (isSameFile(in, f, w, h, m)) {
		fscanf(f3, "%s\n%d %d\n%d\n", f, &w, &h, &m);
		if (isSameFile(in, f, w, h, m)) {
			try {
				in->separated = true;
				in->size = 3 * in->width * in->height;
				in->bytes = new byte[in->size];
				fread(in->bytes, sizeof(byte), in->size / 3, f1);
				fread(in->bytes + in->size / 3, sizeof(byte), in->size / 3, f2);
				fread(in->bytes + 2 * in->size / 3, sizeof(byte), in->size / 3, f3);
				fclose(f1);
				fclose(f2);
				fclose(f3);
				return in;
			}
			catch (std::bad_alloc) {
				throw IllegalArgumentException("Failed to allocate input data");
			}
		}
	}
	throw IllegalArgumentException("Files have inequal size");
}

bool isSameFile(file* in, const char* f, int w, int h, int m) {
	return (!(strcmp(in->format, f)) && (w == in->width) &&
		(h == in->height) && (m == in->maxcolour));
}

components* move(Base && obj) {
	components* holder = new components;
	holder->size = obj.canals->size;
	holder->x = obj.canals->x;
	obj.canals->x = nullptr;
	holder->y = obj.canals->y;
	obj.canals->y = nullptr;
	holder->z = obj.canals->z;
	obj.canals->z = nullptr;
	return holder;
}

components* transformToRGB(args & input)
{
	switch (input.fromColourSpace) {
	case 0: { // RGB itself
		double matrix[12] = {
			0,  1,  0,  0,
			0,  0,  1,  0,
			0,  0,  0,  1
		};
		return move(Simple(input, matrix, false));
	}
	case 1: { // CMY
		double matrix[12] = {
			1, -1,  0,  0,
			1,  0, -1,  0,
			1,  0,  0, -1
		};
		return move(Simple(input, matrix, false));
	}
	case 2: { // YCoCg
		double matrix[12] = {
			0, 1,  1, -1,
			0, 1,  0,  1,
			0, 1, -1, -1
		};
		return move(Simple(input, matrix, true));
	}
	case 3: { // YCbCr.601
		double kr = 0.299, kg = 0.597, kb = 0.114;
		return move(YCbCr(input, kr, kg, kb));
	}
	case 4: { // YCbCr.709
		double kr = 0.2126, kg = 0.7152, kb = 0.0722;
		return move(YCbCr(input, kr, kg, kb));
	}
	case 5: { // HSL
		return move(HSX(input, false));
	}
	case 6: { // HSV
		return move(HSX(input, true));
	}
	default: 
		throw IllegalArgumentException("Colour space does not exists");
	}
}

components* transformFromRGB(args& input, components* RGB)
{
	switch (input.toColourSpace) {
	case 0: { // Again RGB
		double matrix[12] = {
			0,  1,  0,  0,
			0,  0,  1,  0,
			0,  0,  0,  1
		};
		return move(Simple(RGB, matrix, false));
	}
	case 1: { // CMY
		double matrix[12] = {
			1, -1,  0,  0,
			1,  0, -1,  0,
			1,  0,  0, -1
		};
		return move(Simple(RGB, matrix, false));
	}
	case 2: { // YCoCg
		double matrix[12] = {
			0,  0.25, 0.50,  0.25,
			0,  0.50, 0.00, -0.50,
			0, -0.25, 0.50, -0.25
		};
		return move(Simple(RGB, matrix, true));
	}
	case 3: { // YCbCr.601
		double kr = 0.299, kg = 0.597, kb = 0.114;
		return move(YCbCr(RGB, kr, kg, kb));
	}
	case 4: { // YCbCr.709
		double kr = 0.2126, kg = 0.7152, kb = 0.0722;
		return move(YCbCr(RGB, kr, kg, kb));
	}
	case 5: { // HSL
		return move(HSX(RGB, false));
	}
	case 6: { // HSV
		return move(HSX(RGB, true));
	}
	default:
		throw IllegalArgumentException("Colour space does not exists");
	}
}

void clean(components* data) {
	delete[] data->x;
	delete[] data->y;
	delete[] data->z;
	delete data;
}
