#define _CRT_SECURE_NO_WARNINGS
#include<iostream>

typedef unsigned char ch;

class pixel
{
public:

	double delta;
	double onPixel;
	int onPicture;

	pixel(double thickness, double delta, bool upper) {
		double ceiling;
		double thick = modf(thickness / 2 * cosin(delta) - 0.5, &ceiling);
		if (thick <= 0) {
			thick += 0.5;
			onPixel = (upper) ? (0.5 + thick) : (0.5 - thick);
		}
		else {
			onPixel = (upper) ? thick : (1 - thick);
		}
		onPicture = (upper) ? static_cast<int>(-ceiling) : static_cast<int>(ceiling);
		this->delta = delta;
	}

	void move_down() {
		onPicture++;
	}
	void move_next() {
		onPixel -= (isTriangle()) ? (delta - 1) : delta;
	}
	void move() {
		if (isTriangle())
			move_down();
		move_next();
	}

	bool isTriangle() {
		return onPixel <= delta;
	}
	bool oneLevel(const pixel& p) {
		return onPicture == p.onPicture;
	}

	double percentage() {
		if (isTriangle())
			return sqr(onPixel) / delta / 2;
		else
			return (2 * onPixel - delta) / 2;
	}
	double specialTriangle(double x) {
		return sqr(x - onPixel) / delta / 2;
	}

private:

	double cosin(double tg) {
		return 1 / (1 + tg * tg);
	};
	double sqr(double x) {
		return x * x;
	}
};

class point
{
public:

	int length;

	point(ch* origin, int w, int h, int br, int length, double delta, double gamma) :
		point(origin, w, h, length, delta, br) {
		this->gamma = gamma;
		sRGB = (gamma == 0) ? true : false;
	}
	
	void draw_over(pixel& p, int x) {
		percentage = p.percentage();
		draw(x, p.onPicture);
	}
	void draw_under(pixel& p, int x) {
		if (p.isTriangle()) {
			percentage = p.specialTriangle(delta);
			draw(x, p.onPicture + 1);
		}
	}
	void draw_oneLevel(pixel& p1, pixel& p2, int x) {
		percentage = p1.percentage() - p2.percentage();
		draw(x, p1.onPicture);
	}
	void draw_rest(pixel& p1, pixel& p2, int x) {
		if (p1.isTriangle()) {
			double hole = p1.specialTriangle(delta);
			percentage = p2.percentage() - hole;
			draw(x, p2.onPicture);
		}
		else {
			draw_under(p2, x);
		}
	}
	void draw_body(pixel& p1, pixel& p2, int x) {
		pixel temp = p1;
		if (temp.isTriangle()) {
			temp.move();
			if (temp.oneLevel(p2)) {
				double percentage1 = temp.specialTriangle(1);
				double percentage2 = p2.percentage();
				percentage = 1 - (percentage1 + percentage2);
				draw(x, p2.onPicture);
				return;
			}
			else {
				percentage = 1 - temp.specialTriangle(1);
				draw(x, temp.onPicture);
			}
		}
		else {
			temp.move_down();
			if (temp.oneLevel(p2)) {
				percentage = 1 - p2.percentage();
				draw(x, temp.onPicture);
				return;
			}
			else {
				percentage = 1;
				draw(x, temp.onPicture);
			}
		}
		percentage = 1;
		while (!temp.oneLevel(p2)) {
			temp.move_down();
			if (temp.oneLevel(p2)) {
				percentage = 1 - p2.percentage();
			}
			draw(x, temp.onPicture);
		}
	}

private:

	ch* origin;
	int widthoffset;
	int heightoffset;

	double brightness;
	double percentage;
	double delta;

	bool sRGB;
	double gamma;
	const double BOARD = 0.0031308;
	const double INVERSEBOARD = 0.04045;

	point(ch* origin, int w, int h, int length, double delta, int br) :
		widthoffset(w), heightoffset(h), percentage(0) {
		this->origin = origin;
		this->delta = delta;
		this->length = length;
		brightness = static_cast<double>(br);
	}

	ch* get(int x, int y) {
		return origin + x * widthoffset + y * heightoffset;
	}

	void draw(int x, int y) {
		(sRGB) ? gammaSRGB(get(x, y)) : gammaManual(get(x, y));
	}

	double linear(double x, bool inverse) { 
		return (inverse) ? (25 * (x / 255.0)) / 323.0 :
			(323 * x) / 25.0;
	}
	double exponential(double x, bool inverse) {
		return (inverse) ? pow((200 * (x / 255.0) + 11) / 211.0, 12 / 5.0) :
			(211 * pow(x, 5.0 / 12) - 11) / 200.0;
	}

	void gammaSRGB(ch* cur) {
		double colour;

		if (percentage * brightness / 255.0 <= INVERSEBOARD)
		{
			if (*(cur) * (1 - percentage) <= INVERSEBOARD)
				colour = linear(brightness, true) * percentage + linear(*cur, true) * (1 - percentage);
			else
				colour = linear(brightness, true) * percentage + exponential(*cur, true) * (1 - percentage);
		}
		else
		{
			if (*(cur) / 255.0 * (1 - percentage) <= INVERSEBOARD)
				colour = exponential(brightness, true) * percentage + linear(*cur, true) * (1 - percentage);
			else
				colour = exponential(brightness, true) * percentage + exponential(*cur, true) * (1 - percentage);
		}
		if (colour <= BOARD)
			*cur = static_cast<ch>(linear(colour, false) * 255);
		else
			*cur = static_cast<ch>(exponential(colour, false) * 255);
	}
	void gammaManual(ch* cur) {
		double colour;
		colour = pow(brightness / 255.0, gamma) * percentage +
			pow((*cur) / 255.0, gamma) * (1 - percentage);
		*cur = static_cast<ch>((pow(colour, 1 / gamma) * 255));
	}

	
};

void swap(int& x, int& y);
void plot(point& line, pixel& upper, pixel& lower);

int main(int argc, char* argv[])
{
	if ((argc < 9) || (argc > 10)) {
		std::cerr << "Invalid number of args";
		return 1;
	}
	int width, height, maxcolour;
	FILE* picture, * out;
	char format[3];
	picture = fopen(argv[1], "rb");
	out = fopen(argv[2], "wb");
	if ((picture == nullptr) || (out == nullptr)) {
		std::cerr << "File problems";
		return 1;
	}
	fscanf(picture, "%s %d %d %d\n", &format, &width, &height, &maxcolour);
	if (strcmp(format, "P5")) {
		std::cerr << "Unknown format";
		return 1;
	}
	ch* data;
	try {
		data = new ch[width * height];
		fread(data, 1, width * height, picture);
	}
	catch (std::bad_alloc e) {
		std::cerr << "Out of memory";
		return 1;
	}
	int brightness = atoi(argv[3]);
	double thickness = atof(argv[4]);
	int x0 = atoi(argv[5]), y0 = atoi(argv[6]),
		x1 = atoi(argv[7]), y1 = atoi(argv[8]);
	double gamma = (argc == 9) ? 0 : atof(argv[9]);
	int length, w, h;
	double delta;
	if (abs(x1 - x0) >= abs(y1 - y0)) {
		length = abs(x1 - x0);
		if (x0 > x1) {
			swap(x0, x1);
			swap(y0, y1);
		}
		if (y0 > y1) {
			delta = static_cast<double>(y0 - y1) / (x1 - x0 + 1);
			w = 1, h = -width;
		}
		else {
			delta = static_cast<double>(y1 - y0) / (x1 - x0 + 1);
			w = 1, h = width;
		}
	}
	else {
		length = abs(y1 - y0);
		if (y0 > y1) {
			swap(x0, x1);
			swap(y0, y1);
		}
		if (x0 > x1) {
			delta = static_cast<double>(x0 - x1) / (y1 - y0 + 1);
			w = width, h = -1;
		}
		else {
			delta = static_cast<double>(x1 - x0) / (y1 - y0 + 1);
			w = width, h = 1;
		}
	}
	ch* origin = data + y0 * width + x0;
	point line = point(origin, w, h, brightness, length, delta, gamma);
	pixel upper = pixel(thickness, delta, true);
	pixel lower = pixel(thickness, delta, false);
	plot(line, upper, lower);
	fprintf(out, "%s\n%d %d\n%d\n", format, width, height, maxcolour);
	fwrite(data, 1, width * height, out);
	delete[] data;
	fclose(picture);
	fclose(out);
	return 0;
}

void swap(int& x, int& y) {
	x = x + y;
	y = x - y;
	x = x - y;
}

void plot(point& line, pixel& upper, pixel& lower) 
{
	for (int x = 0; x <= line.length; x++) {
		if (!upper.oneLevel(lower)) {
			line.draw_over(upper, x);
			line.draw_body(upper, lower, x);
			line.draw_under(lower, x);
		}
		else {
			line.draw_oneLevel(upper, lower, x);
			if(lower.isTriangle()) {
				line.draw_rest(upper, lower, x);
			}
		}
		upper.move();
		lower.move();
	}
}
