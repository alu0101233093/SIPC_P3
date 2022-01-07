#include <iostream>
#include <cmath>
#include <cstring>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>

using namespace cv;
using namespace std;

enum opciones {Menu, Grabar, Leer = 3};

// Calcula el ángulo entre los vectores
// (f, s) y (f, e).
double angle(Point s, Point e, Point f) {
    double v1[2],v2[2];
    v1[0] = s.x - f.x;
    v1[1] = s.y - f.y;
    v2[0] = e.x - f.x;
    v2[1] = e.y - f.y;
    double ang1 = atan2(v1[1], v1[0]);
    double ang2 = atan2(v2[1], v2[0]);

    double ang = ang1 - ang2;
    if (ang > CV_PI) ang -= 2*CV_PI;
    if (ang < -CV_PI) ang += 2*CV_PI;

    return ang*180/CV_PI;
}

int find_biggest_contour(vector<vector<Point> > contours) {
	int index = 0;
	
	for (int i = 0; i < contours.size(); i++) {
			if (contours[i].size() > contours[index].size())
				index = i;
		}

	return index;
}

void mensajes (int opt) {
	switch (opt) {
		case Menu:
			cout << "PRÁCTICA 3: DISPOSITIVOS DE INTERACCIÓN GESTUALES." << endl << endl;
			cout << "\tJaime Simeón Palomar Blumenthal" << endl;
			cout << "\tNoelia Ibañez Silvestre" << endl;
			cout << "\tLeonardo Alfonso Cruz Rodríguez" << endl << endl;

			cout << "<-- MENÚ -->" << endl;
			cout << "1. Grabar vídeo." << endl;
			cout << "2. Comprobar tasa de fps de la cámara." << endl;
			cout << "3. Leer archivo de vídeo." << endl;
			cout << "9. Salir" << endl << endl;
			cout << "Teclee la opción que desee (1, 2, 3, ...): ";
		break;
		
		case Grabar:
			cout << "<-- GRABACIÓN DE VÍDEO -->" << endl;
			cout << "- Presione 'q' para salir." << endl;
			cout << "- Presione 'r' para comenzar/terminar la grabación." << endl << endl;
			cout << "El fichero de vídeo de salida se llama 'out.avi'." << endl << endl;
		break;

		case Leer:
			cout << "<-- LECTURA DESDE UN FICHERO DE VÍDEO -->" << endl;
			cout << "Presione 'q' para salir." << endl;
			cout << "Presione 'p' para pausar/despausar la grabación." << endl << endl;
			cout << "Indica el nombre del fichero: ";
		break;

	}
}

int media_fps(VideoCapture cap, Mat frame) {
	int num_frames = 120;
	time_t start, end;

	cap.open(0);

	if (!cap.isOpened()) {
		printf("Error al abrir la cámara\n");
		return -1;
	}

	cout << "Se están capturando " << num_frames << " frames." << endl;

	time(&start);

	for (int i = 0; i < num_frames; i++) {
		cap >> frame;
	}

	time(&end);

	double seconds = difftime(end, start);
	cout << "Tiempo total: " << seconds << " segundos." << endl;

	double fps = num_frames / seconds;

	cout << "Frames por segundo estimados: " << fps << endl;

	cap.release();

	return 0;
}

int record_video(VideoCapture cap, Mat frame, Mat roi, Mat fgMask, Mat blurred, Rect rect, Ptr<BackgroundSubtractor> pBackSub) {
	mensajes(Grabar);	

	cap.open(0);

	if (!cap.isOpened()) {
		printf("Error al abrir la cámara\n");
		return -1;
	}

	int frame_width = cap.get(CAP_PROP_FRAME_WIDTH); 
	int frame_height = cap.get(CAP_PROP_FRAME_HEIGHT); 
	
	int codec = VideoWriter::fourcc('M','J','P','G');
	VideoWriter video("out.avi",codec,24, Size(frame_width,frame_height));

	vector<vector<Point> > contours;
	vector<float> dedos(24);
	vector<float> proporciones(24);
	int j = 0;

	namedWindow("Frame");
	namedWindow("ROI");

	// Variables modificables por el usuario en tiempo de ejecución.
	bool exit = false;
	bool debug = false;
	bool record = false;
	int lr = -1;

	while(exit == false) {
		cap >> frame;

		flip(frame, frame, 1);

		if (record == true) video.write(frame);

		// Se difumina la imagen de la ROI para eliminar
		// pequelas variaciones de la lente que son
		// interpretadas como movimiento.
		GaussianBlur(frame, blurred, Size(13, 13), 0);
		blurred(rect).copyTo(roi);

		pBackSub->apply(roi, fgMask, lr);

		// Crea los contornos y se busca el mayor.
		findContours(fgMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		int index = find_biggest_contour(contours);

		// La malla más grande se define a partir de los
		// índices del contorno más grande
		vector<int> hull;
		convexHull(contours[index], hull, false, false);
		sort(hull.begin(), hull.end(), greater <int>());

		// Dibuja el contorno y la malla más grandes
		drawContours(roi, contours, index, Scalar(0, 255, 0), 3);

		vector<Vec4i> defects;
		convexityDefects(contours[index], hull, defects);

		// Dibuja el rectángulo de área mínima y el del ROI.
		Rect boundRect = boundingRect(contours[index]);
		rectangle(roi,boundRect,Scalar(0,0,255),3);
		rectangle(frame, rect, Scalar(255, 0, 0));

		// Calcula la proporción entre el ancho y el alto del
		// rectángulo de área mínima.
		float proportion = (float)boundRect.width / (float)boundRect.height;

		// Contador de dedos
		int ctr = 0;

		// Calcula puntos s, e, f, ángulo y profundidad.
		for (int i = 0; i < defects.size(); i++) {
    		Point s = contours[index][defects[i][0]];
        	Point e = contours[index][defects[i][1]];
        	Point f = contours[index][defects[i][2]];
        	float depth = ((float)defects[i][3] / 256.0) / boundRect.width;
        	double ang = angle(s,e,f);

			if (depth > 0.02 && ang < 100) {
				circle(roi, f,5,Scalar(0,0,255),-1);
        		line(roi,s,e,Scalar(255,0,0),2);
				ctr++;
			}
    	}

		float sum_proporciones = 0;

		proporciones[j] = proportion;

		// Si ha encontrado defectos:
		// 		Llena un vector con los dedos que se han
		// 		mostrado en cada frame y se hace la media.

		// Si no:
		//		Utiliza las proporciones del rectángulo de
		//		área mínima para determinar si hay uno o
		//		ningún dedo.
		if (ctr != 0) ctr++;

		else {
			for (int i = 0; i < proporciones.size(); i++)
				sum_proporciones += proporciones[i];
			

			float media_proporciones = sum_proporciones / proporciones.size();

			if (media_proporciones >= 0.7)
				ctr = 1;
		}
		
		dedos[j] = ctr;

		j++;

		if (j > dedos.size()) j = 0;

		float sum_dedos = 0;

		for (int i = 0; i < dedos.size(); i++)
			sum_dedos += dedos[i];

		int media_int = trunc(sum_dedos / dedos.size());

		// Se muestra el número de dedos levantados en pantalla.
		string media = "Dedos levantados: ";
		media += to_string(media_int);

		vector<string> instrucciones(4);
		instrucciones[0] = "Salir: q";
		instrucciones[1] = "Apagar Learning Rate: l";
		instrucciones[2] = "Grabar: r";
		instrucciones[3] = "Debug (Mostrar medias): d";

		putText(frame, media, Point(10, 50), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 0, 0), 2);

		int altura = 300;

		for (int i = 0; i < instrucciones.size(); i++) {
			putText(frame, instrucciones[i], Point(10, altura), FONT_HERSHEY_DUPLEX, 0.5, CV_RGB(255, 255, 0), 1);

			altura += 50;
		}

		imshow("Foreground Mask",fgMask);
		imshow("ROI", roi);
		imshow("Frame",frame);

		int c = waitKey(40);

		// q -> Salir
		// l -> Learning rate apagado
		// r -> Grabar
		// d -> Debug (Muestra medias)
		switch (c) {
			case 'q':
				exit = true;
			break;

			case 'l':
				lr = 0;
			break;

			case 'r':
				record = !record;
			break;

			case 'd':
				debug = !debug;
			break;
		}

		if (debug == true) {
			cout << "Media: " << sum_dedos / dedos.size() << endl;
			cout << "Media truncada: " << media_int << endl << endl;
		}
	}

	cap.release();
	destroyAllWindows();

	return 0;
}

int read_from_video(VideoCapture cap, Mat frame, Mat roi, Mat fgMask, Mat blurred, Rect rect, Ptr<BackgroundSubtractor> pBackSub) {
	mensajes(Leer);
	
	basic_string<char> name;
	cin >> name;

	cout << endl << endl;
	
	cap.open(name);

	if (!cap.isOpened()) {
		printf("Error al abrir el fichero\n");
		return -1;
	}

	vector<vector<Point> > contours;
	vector<float> dedos(24);
	vector<float> proporciones(24);
	int j = 0;

	namedWindow("Frame");
	namedWindow("ROI");
	namedWindow("Foreground Mask");

	bool pause = false;
	bool exit = false;
	bool debug = false;
	int lr = -1;

	while(exit == false) {
		if (pause == false) cap >> frame;

		if (frame.empty()) break;

		// Se difumina la imagen de la ROI para eliminar
		// pequelas variaciones de la lente que son
		// interpretadas como movimiento.
		GaussianBlur(frame, blurred, Size(13, 13), 0);
		blurred(rect).copyTo(roi);

		pBackSub->apply(roi, fgMask, lr);

		// Crea los contornos y se busca el mayor.
		findContours(fgMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		int index = find_biggest_contour(contours);

		// La malla más grande se define a partir de los
		// índices del contorno más grande
		vector<int> hull;
		convexHull(contours[index], hull, false, false);
		sort(hull.begin(), hull.end(), greater <int>());

		// Dibuja el contorno y la malla más grandes
		drawContours(roi, contours, index, Scalar(0, 255, 0), 3);

		// Calcula los defectos de convexidad.
		vector<Vec4i> defects;
		convexityDefects(contours[index], hull, defects);

		// Dibuja el rectángulo de área mínima y el del ROI.
		Rect boundRect = boundingRect(contours[index]);
		rectangle(roi,boundRect,Scalar(0,0,255),3);
		rectangle(frame, rect, Scalar(255, 0, 0));

		// Calcula la proporción entre el ancho y el alto del
		// rectángulo de área mínima.
		float proportion = proportion = (float)boundRect.width / (float)boundRect.height;

		// Calcula puntos s, e, f, ángulo y profundidad.
		int ctr = 0;

		for (int i = 0; i < defects.size(); i++) {
    		Point s = contours[index][defects[i][0]];
        	Point e = contours[index][defects[i][1]];
        	Point f = contours[index][defects[i][2]];
        	float depth = ((float)defects[i][3] / 256.0) / boundRect.width;
        	double ang = angle(s,e,f);
        	
			if (depth > 0.02 && ang < 100) {
				circle(roi, f,5,Scalar(0,0,255),-1);
        		line(roi,s,e,Scalar(255,0,0),2);
				ctr++;
			}
    	}

		float sum_proporciones = 0;

		proporciones[j] = proportion;

		// Si ha encontrado defectos:
		// 		Llena un vector con los dedos que se han
		// 		mostrado en cada frame y se hace la media.

		// Si no:
		//		Utiliza las proporciones del rectángulo de
		//		área mínima para determinar si hay uno o
		//		ningún dedo.
		if (ctr != 0) ctr++;

		else {
			for (int i = 0; i < proporciones.size(); i++)
				sum_proporciones += proporciones[i];
			

			float media_proporciones = sum_proporciones / proporciones.size();

			if (media_proporciones >= 0.7)
				ctr = 1;
		}
		
		dedos[j] = ctr;

		j++;

		if (j > dedos.size()) j = 0;

		float sum_dedos = 0;

		for (int i = 0; i < dedos.size(); i++)
			sum_dedos += dedos[i];

		int media_int = trunc(sum_dedos / dedos.size());

		// Se muestra el número de dedos levantados en pantalla.
		string media = "Dedos levantados: ";
		media += to_string(media_int);

		vector<string> instrucciones(4);
		instrucciones[0]= "Salir: q";
		instrucciones[1] = "Apagar Learning Rate: l";
		instrucciones[2] = "Grabar: r";
		instrucciones[3] = "Debug (Mostrar medias): d";

		putText(frame, media, Point(10, 50), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 0, 0), 2);

		int altura = 300;

		for (int i = 0; i < instrucciones.size(); i++) {
			putText(frame, instrucciones[i], Point(10, altura), FONT_HERSHEY_DUPLEX, 0.5, CV_RGB(255, 255, 0), 1);

			altura += 50;
		}

		imshow("Frame",frame);
		imshow("ROI", roi);
		imshow("Foreground Mask", fgMask);

		int c = waitKey(40);

		// q -> Salir
		// l -> Learning rate apagado
		// r -> Grabar
		// d -> Debug (Muestra medias)
		switch (c) {
			case 'q':
				exit = true;
			break;

			case 'l':
				lr = 0;
			break;

			case 'p':
				pause = !pause;
			break;

			case 'd':
				debug = !debug;
			break;
		}

		if (debug == true) {
			cout << "Media: " << sum_dedos / dedos.size() << endl;
			cout << "Media truncada: " << media_int << endl << endl;
		}
	}

	cap.release();
	destroyAllWindows();

	return 0;
}


int main(int argc, char* argv[])
{
	Mat frame, roi, fgMask, blurred;
	VideoCapture cap;
	Rect rect(400, 100, 200, 200);
	Ptr<BackgroundSubtractor> pBackSub=createBackgroundSubtractorMOG2();

	mensajes(Menu);

	int opcion;
	cin >> opcion;

	cout << endl;

	switch (opcion)
	{	case 1:
			record_video(cap, frame, roi, fgMask, blurred, rect, pBackSub);
		break;

		case 2:
			media_fps(cap, frame);
		break;

		case 3:
			read_from_video(cap, frame, roi, fgMask, blurred, rect, pBackSub);
		break;

		default:
			cout << "Saliendo..." << endl;
		break;
	}
}