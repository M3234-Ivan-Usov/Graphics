#define _CRT_SECURE_NO_WARNINGS
#define EXCLUDE 0.0039
#include<iostream>

typedef unsigned char byte;

struct file {
	char format[3];
	int width, height;
	int maxcolour, size;

	byte* data;

	int offset;
	double multiplicator;
};

struct components {
	int size;
	double* x;
	double* y;
	double* z;
};

class Brightness {
public:

	components* canals;

	Brightness(file* picture, bool toY, bool advanced) :
		Brightness(picture->data, picture->size, toY) {
		this->advanced = advanced;
		analyse();
	}
	Brightness(file* picture, bool toY) :
		Brightness(picture->data, picture->size, toY) {
		offset = picture->offset;
		multiplicator = picture->multiplicator;
		if (isYCbCr) {
			toYCbCr();
		}
	}
	~Brightness() {
		delete[] canals->x;
		delete[] canals->y;
		delete[] canals->z;
		delete canals;
	}

	void run() {
		if (isYCbCr) {
			for (int i = 0; i < canals->size; i++) {
				canals->x[i] = (canals->x[i] - offset) * multiplicator;
			}
			toRGB();
		}
		else {
			for (int i = 0; i < canals->size; i++) {
				canals->x[i] = (canals->x[i] - offset) * multiplicator;
				canals->y[i] = (canals->y[i] - offset) * multiplicator;
				canals->z[i] = (canals->z[i] - offset) * multiplicator;
			}
		}
	}
	void put(FILE* destination, file* meta) {
		fprintf(destination, "%s\n%d %d\n%d\n",
			meta->format, meta->width, meta->height, meta->maxcolour);
		for (int i = 0; i < meta->size; i++) {
			switch (i % 3) {
			case 0:
				meta->data[i] = normalise(canals->x[i / 3]);
				break;
			case 1:
				meta->data[i] = normalise(canals->y[i / 3]);
				break;
			case 2:
				meta->data[i] = normalise(canals->z[i / 3]);
				break;
			}
		}
		fwrite(meta->data, sizeof(byte), meta->size, destination);
		delete[] meta->data;
		delete meta;
	}

private:

	const double kr = 0.299;
	const double kg = 0.587;
	const double kb = 0.114;

	bool isYCbCr;
	bool advanced;

	int offset;
	double multiplicator;

	Brightness(byte* data, int size, bool toY) : isYCbCr(toY) {
		canals = new components;
		canals->size = size / 3;
		canals->x = new double[canals->size];
		canals->y = new double[canals->size];
		canals->z = new double[canals->size];
		for (int i = 0; i < size; i++) {
			switch (i % 3) {
			case 0: {
				canals->x[i / 3] = data[i];
				break;
			}
			case 1: {
				canals->y[i / 3] = data[i];
				break;
			}
			case 2: {
				canals->z[i / 3] = data[i];
				break;
			}
			}
		}
	}

	void analyse() {
		int* analyser = new int[256];
		int k;
		for (int i = 0; i < 256; i++) {
			analyser[i] = 0;
		}
		if (isYCbCr) {
			toYCbCr();
			for (int i = 0; i < canals->size; i++) {
				analyser[(byte)canals->x[i]]++;
			}
			k = 1;
		}
		else {
			for (int i = 0; i < canals->size; i++) {
				analyser[(byte)canals->x[i]]++;
				analyser[(byte)canals->y[i]]++;
				analyser[(byte)canals->z[i]]++;
			}
			k = 3;
		}
		int min = 0, max = 255;
		int exclusions = (advanced) ? (int)(k * canals->size * EXCLUDE) : 0;
		int i = 0;
		do {
			while (analyser[min] == 0) {
				min++;
			}
			analyser[min]--;
			while (analyser[max] == 0) {
				max--;
			}
			analyser[max]--;
			i++;
		} while (i <= exclusions);
		offset = min;
		multiplicator = 255.0 / (max - min);
		std::cout << "Offset: " << min << std::endl << "Multiplicator: " << multiplicator;
		delete[] analyser;
	}
	byte normalise(double value) {
		if (value > 255) {
			return 255;
		}
		if (value < 0) {
			return 0;
		}
		return (byte)value;
	}

	void toYCbCr() {
		double tempX, tempY, tempZ;
		double R, G, B;
		double c[9] = {
			kr, kg, kb,
			kr / (2 * kb - 2), kg / (2 * kb - 2), 0.5,
			0.5, kg / (2 * kr - 2), kb / (2 * kr - 2)
		};
		for (int i = 0; i < canals->size; i++) {
			R = canals->x[i] / 255.0;
			G = canals->y[i] / 255.0;
			B = canals->z[i] / 255.0;
			tempX = c[0] * R + c[1] * G + c[2] * B;
			tempY = c[3] * R + c[4] * G + c[5] * B;
			tempZ = c[6] * R + c[7] * G + c[8] * B;
			canals->x[i] = tempX * 255;
			canals->y[i] = tempY * 255 + 128;
			canals->z[i] = tempZ * 255 + 128;
		}
	}
	void toRGB() {
		double tempX, tempY, tempZ;
		double Y, Cb, Cr;
		double c[9] = {
			1, 0, 2 - 2 * kr,
			1, (kb / kg) * (2 * kb - 2), (kr / kg) * (2 * kr - 2),
			1, 2 - 2 * kb, 0
		};
		for (int i = 0; i < canals->size; i++) {
			Y = canals->x[i] / 255.0;
			Cb = (canals->y[i] - 128) / 255.0;
			Cr = (canals->z[i] - 128) / 255.0;
			tempX = c[0] * Y + c[1] * Cb + c[2] * Cr;
			tempY = c[3] * Y + c[4] * Cb + c[5] * Cr;
			tempZ = c[6] * Y + c[7] * Cb + c[8] * Cr;
			canals->x[i] = tempX * 255;
			canals->y[i] = tempY * 255;
			canals->z[i] = tempZ * 255;
		}
	}
};

file* parse(FILE* in);

int main(int argc, char** argv) {
	FILE* in = fopen(argv[1], "rb");
	FILE* out = fopen(argv[2], "wb");
	if ((in == NULL) || (out == NULL)) {
		std::cerr << "Failed to open/create file" << std::endl;
		return 1;
	}
	file* picture = parse(in);
	if (picture == nullptr) {
		std::cerr << "Failed to parse file";
		return 1;
	}
	Brightness* correction;
	int mode = atoi(argv[3]);
	switch (mode) {
	case 0: {
		if (argc == 6) {
			picture->offset = atoi(argv[4]);
			picture->multiplicator = atof(argv[5]);
			correction = new Brightness(picture, false);
		}
		else {
			std::cerr << "Incorrect number of args";
			return 1;
		}
		break;
	}
	case 1: {
		if (argc == 6) {
			picture->offset = atoi(argv[4]);
			picture->multiplicator = atof(argv[5]);
			correction = new Brightness(picture, true);
		}
		else {
			std::cerr << "Incorrect number of args";
			return 1;
		}
		break;
	}
	case 2: {
		correction = new Brightness(picture, false, false);
		break;
	}
	case 3: {
		correction = new Brightness(picture, true, false);
		break;
	}
	case 4: {
		correction = new Brightness(picture, false, true);
		break;
	}
	case 5: {
		correction = new Brightness(picture, true, true);
		break;
	}
	default: {
		std::cerr << "Incorrect correction mode";
		return 1;
	}
	}
	correction->run();
	correction->put(out, picture);
	delete correction;
}

file* parse(FILE* in) {
	file* picture = new file;;
	fscanf(in, "%s\n%d %d\n%d\n", picture->format,
		&picture->width, &picture->height, &picture->maxcolour);
	if (!strcmp(picture->format, "P6")) {
		picture->size = 3 * picture->width * picture->height;;
	}
	else {
		return nullptr;
	}
	try {
		picture->data = new byte[picture->size];
		fread(picture->data, sizeof(byte), picture->size, in);
	}
	catch (std::bad_alloc e) {
		std::cerr << e.what();
		return nullptr;
	}
	return picture;
}
