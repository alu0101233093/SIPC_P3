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
		cap>>frame;

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