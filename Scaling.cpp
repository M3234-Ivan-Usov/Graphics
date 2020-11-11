#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#include<iostream>
#include<exception>
#include<future>
#include<vector>
#include<iomanip>
#include<chrono>
#include<cmath>
#include<cstdlib>

typedef unsigned char byte;
using namespace std;

struct input;
class Base; //abstract parent for each class can run async with
//flag -threads <number> after param C-Spline. By default is 4
class Neighbour; //does not use convolution
class Bilinear; //implicitly uses convolution
class Convolution; //abstract for using convolution
class Lanczos;
class BC_Splines; 
class Bell;
class Hermite;

input* parse(int argc, char** argv); //throws std::exception if incorrect args
Base* getInstance(input* args); //returns resampling-appropriate pointer
void proxy(Base* ptr, int b, int e); //not a member-function to run async
void clear(input* a, Base* w); //closes files and deletes object

struct input {
	FILE* in = nullptr;
	FILE* out = nullptr;

	int originalX = 0;
	int originalY = 0;
	int scaledX = 0;
	int scaledY = 0;

	int originalSize = 0;
	int scaledSize = 0;

	int type = 0;

	byte* image = nullptr;

	//optional
	double B_Spline = 0.0;
	double C_Spline = 0.5;
	int proportional;
	int threads = 4;

	//are not in use
	double dx = 0.0;
	double dy = 0.0;
	bool sRGB = true;
	double gamma = 0.0;
};

class Base {
public:
	void run(int threads) {
		cout << "Threads: " << threads << endl;
		auto distribution = getDistribution(threads);
		auto results = vector<future<void>>(threads);
		for (int i = 1; i <= threads; i++) {
			results[i - 1] = async(launch::async, proxy, this, distribution[i - 1], distribution[i]);
		}
		for (int i = 0; i < threads; i++) {
			results[i].get();
		}
	}

	void run() {
		this->start(-1, meta->scaledSize);
	}

	void fill() {
		fprintf(meta->out, "%s\n%d %d\n%d\n", "P5", meta->scaledX, meta->scaledY, 255);
		fwrite(image, sizeof(byte), meta->scaledSize, meta->out);
	}

	virtual void start(int begin, int end) = 0;

	virtual ~Base() {
		delete[] image;
	}
protected:
	Base(input* args) {
		meta = args;
		scalingX = (double)meta->originalX / (double)meta->scaledX;
		scalingY = (double)meta->originalY / (double)meta->scaledY;
		cout << "Width scale factor: " << setprecision(3) << 1 / scalingX * 100 << "%" << endl;
		cout << "Height scale factor: " << setprecision(3) << 1 / scalingY * 100 << "%" << endl;
		image = new byte[args->scaledSize];
	}

	input* meta;
	byte* image;

	double scalingX;
	double scalingY;

	byte get(int y, int x) {
		return meta->image[meta->originalX * y + x];
	}
	byte normalise(double pixel) {
		if (pixel < 0)
			return 0;
		if (pixel > 255)
			return 255;
		return static_cast<byte>(pixel);
	}
private:
	vector<int> getDistribution(int threads) {
		int perWorker = meta->scaledSize / threads;
		int remain = meta->scaledSize % threads;
		auto workAmount = vector<int>(threads + 1);
		for (int i = 1; i <= threads; i++) {
			workAmount[i] = workAmount[i - 1] + perWorker + ((remain > 0) ? 1 : 0);
			remain--;
		}
		workAmount[0]--;
		return workAmount;
	}
};

class Neighbour : public Base {
public:
	Neighbour(input *args) : Base(args) {};

	void start(int begin, int end) {
		for (int i = begin + 1; i < end; i++) {
			int y = (i / meta->scaledX) * scalingY;
			int x = (i % meta->scaledX) * scalingX;
			image[i] = get(y, x);
		}
	}
};

class Bilinear : public Base {
public:
	Bilinear(input* args) : Base(args) {}

	void start(int begin, int end) {
		double floor, fracX, fracY, upper, lower;
		for (int i = begin + 1; i < end; i++) {
			fracY = modf((i / meta->scaledX) * scalingY, &floor);
			int y = (int)floor;
			fracX = modf((i % meta->scaledX) * scalingX, &floor);
			int x = (int)floor;
			bool horizontalBoard = (x > meta->originalX - 2);
			bool verticalBoard = (y > meta->originalY - 2);
			if (horizontalBoard)
				--x;
			if (verticalBoard)
				--y;
			upper = (1 - fracX) * get(y, x) + fracX * get(y, x + 1);
			lower = (1 - fracX) * get(y + 1, x) + fracX * get(y + 1, x + 1);
			image[i] = normalise(upper * (1 - fracY) + lower * fracY);
		}
	}
};

class Convolution : public Base {
public:
	Convolution(input* args, int order) : Base(args), ORDER(order) {}
	void start(int begin, int end) {
		double floor, fracX, fracY;
		double sumWeight, sumPixel;
		bool horizontalBoard, verticalBoard;
		int y, x;
		for (int k = begin + 1; k < end; k++) {
			fracY = modf((k / meta->scaledX) * scalingY, &floor);
			y = (int)floor;
			fracX = modf((k % meta->scaledX) * scalingX, &floor);
			x = (int)floor;
			if (x < ORDER - 1)
				x = ORDER - 1;
			if (x >= meta->originalX - ORDER)
				x = meta->originalX - ORDER - 1;
			if (y < ORDER - 1)
				y = ORDER - 1;
			if (y >= meta->originalY - ORDER)
				y = meta->originalY - ORDER - 1;
			sumWeight = 0;
			sumPixel = 0;
			for (int i = -ORDER + 1; i <= ORDER; i++) {
				for (int j = -ORDER + 1; j <= ORDER; j++) {
					double curWeight = kernel((double)i - fracX) * kernel((double)j - fracY);
					sumWeight += curWeight;
					sumPixel += get(y + j, x + i) * curWeight;
				}
			}
			image[k] = normalise(sumPixel / sumWeight);
		}
	}
protected:
	const int ORDER;
	virtual double kernel(double v) = 0;
};

class Lanczos : public Convolution {
public:
	Lanczos(input* args) : Convolution(args, 3) {}
protected:
	double kernel(double v) {
		if (v > (double)ORDER) 
			return 0.0;
		if (v == 0)
			return 1.0;
		return sin(M_PI * v) / (M_PI * v) * 
			sin(M_PI * v / ORDER) / (M_PI * v / ORDER);
	}
};

class BC_Splines : public Convolution {
public:
	BC_Splines(input* args) : Convolution(args, 3) {
		double B = args->B_Spline;
		double C = args->C_Spline;
		upperKernel[0] = 6.0 - 2 * B;
		upperKernel[1] = 0.0;
		upperKernel[2] = -18.0 + 12 * B + 6 * C;
		upperKernel[3] = 12.0 - 9 * B - 6 * C;;
		lowerKernel[0] = 8 * B + 24 * C;
		lowerKernel[1] = -12 * B - 48 * C;
		lowerKernel[2] = 6 * B + 30 * C;
		lowerKernel[3] = -B - 6 * C;
		for (int i = 0; i < 4; i++) {
			upperKernel[i] /= 6.0;
			lowerKernel[i] /= 6.0;
		}

	}
private:
	double upperKernel[4];
	double lowerKernel[4];
	double kernel(double v) {
		double x = abs(v);
		if (x < 1) {
			return upperKernel[0] +
				upperKernel[1] * x +
				upperKernel[2] * x * x +
				upperKernel[3] * x * x * x;
		}
		else if (x < 2) {
			return lowerKernel[0] +
				lowerKernel[1] * x +
				lowerKernel[2] * x * x +
				lowerKernel[3] * x * x * x;
		}
		else
			return 0.0;
	}
};

class Bell : public Convolution {
public:
	Bell(input* args) : Convolution(args, 2) {}
protected:
	double kernel(double v) {
		double x = abs(v);
		if (x < 0.5)
			return 0.75 - x * x;
		else if (x < 1.5)
			return 0.5 * (x - 1.5) * (x - 1.5);
		else
			return 0.0;
	}
};

class Hermite : public Convolution {
public:
	Hermite(input* args) : Convolution(args, 3) {}
protected:
	double kernel(double v) {
		double x = abs(v);
		if (x <= 1)
			return 2 * x * x * x - 3 * x * x + 1;
		else
			return 0.0;
	}
};

void proxy(Base* ptr, int begin, int end) {
	ptr->start(begin, end);
}

int main(int argc, char** argv)
{
	try {
		auto begin = chrono::steady_clock::now();
		input* args = parse(argc, argv);
		cout << "Source image size: " << setprecision(5) << 
			args->originalSize / (1024.0 * 1024.0) << " MB" << endl;
		cout << "Scaled image size: " << setprecision(5) <<
			args->scaledSize / (1024.0 * 1024.0) << " MB" << endl;
		Base* worker = getInstance(args);
		(args->threads != 1) ? worker->run(args->threads) : worker->run();
		worker->fill();
		clear(args, worker);
		auto end = chrono::steady_clock::now();
		auto time = chrono::duration_cast<chrono::milliseconds>(end - begin);
		cout << "Finished in " << time.count() << " ms" << endl;
		return 0;
	}
	catch (exception e) {
		e.what();
		return 1;
	}
}

void clear(input* a, Base* w) {
	fclose(a->in);
	fclose(a->out);
	delete[] a->image;
	delete a;
	delete w;
}

input* parse(int argc, char** argv) {
	if ((argc < 9) || (argc > 15))
		throw exception("Number of arguments is bad");
	input* args = new input;
	args->in = fopen(argv[1], "rb");
	args-> out = fopen(argv[2], "wb");
	if ((args->in == NULL) || (args->out == NULL))
		throw exception("File problems");
	args->scaledX = atoi(argv[3]);
	args->scaledY = atoi(argv[4]);
	args->scaledSize = args->scaledX * args->scaledY;
	args->dx = atof(argv[5]);
	args->dy = atof(argv[6]);
	double gamma = atof(argv[7]);
	if (gamma != 0.0) {
		args->gamma = gamma;
		args->sRGB = false;
	}
	args->type = atoi(argv[8]);
	if ((args->type < 0) || (args->type > 5))
		throw exception("Unknown scaling");
	if (args->type == 3) {
		if (argc >= 10) {
			args->B_Spline = atof(argv[9]);
		}
		if (argc >= 11) {
			args->C_Spline = atof(argv[10]);
		}
	}
	char format[3];
	int max;
	fscanf(args->in, "%s\n%d %d\n%d\n", format, &args->originalX, &args->originalY, &max);
	if (!strcmp(format, "P5")) {
		args->originalSize = args->originalX * args->originalY;
	}
	else
		throw exception("Not a PNG");
	try {
		args->image = new byte[args->originalSize];
		fread(args->image, sizeof(byte), args->originalSize, args->in);
	} 
	catch (std::bad_alloc e) {
		throw exception("Failed to allocate input image");
	}
	if (argc >= 13) {
		if (!strcmp(argv[11], "-threads")) {
			args->threads = atoi(argv[12]);
			if (args->threads < 1)
				throw exception("Illegal number of threads");
		}
	}
	if (argc == 15) {
		if (!strcmp(argv[13], "-prop")) {
			args->proportional = atoi(argv[14]);
			args->scaledX = args->proportional / 100.0 * args->originalX;
			args->scaledY = args->proportional / 100.0 * args->originalY;
			args->scaledSize = args->scaledX * args->scaledY;
		}
	}
	return args;
}

Base* getInstance(input* args) {
	switch (args->type) {
	case 0:
		cout << "Nearest Neighbour Resampling" << endl;
		return new Neighbour(args);
	case 1:
		cout << "Bilinear Resampling" << endl;
		return new Bilinear(args);
	case 2:
		cout << "Lanczos3 Resampling" << endl;
		return new Lanczos(args);
	case 3:
		cout << "Bicubic Resampling with B-Spline = " << args->B_Spline << 
			" and C-Spline = " << args->C_Spline << endl;
		return new BC_Splines(args);
	case 4:
		cout << "Bell Resampling" << endl;
		return new Bell(args);
	case 5:
		cout << "Hermite Resampling" << endl;
		return new Hermite(args);
	default:
		return nullptr;
	}
}