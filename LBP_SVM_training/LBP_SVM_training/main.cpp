// This file extracts the LBP-HF feature of face images in the training sets, and train the model via SVM
// Autuor:  Liang Li

#include "cv.h"
#include "ml.h"
#include "highgui.h"
#include "math.h"
#include "iostream"
#include <stdio.h>
#include <math.h>
#include <cxcore.h>

void FFT_Calculate_OneNode(int k);
void FFT_Calculate();
void re_initial();
void LBP_HF(IplImage* image, int num);

using namespace std;
using namespace cv;

int rotate_map[7][8]={{1,2,4,8,16,32,64,128},
                                  {3,6,12,24,48,96,192,129},
                                  {131,7,14,28,56,112,224,193},
                                  {135,15,30,60,120,240,225,195},
                                  {199,143,31,62,124,248,241,227},
                                  {207,159,63,126,252,249,243,231},
                                  {239,223,191,127,254,253,251,247}};

int AU_feature[30]={9,10,11,12,13,14,17,18,21,22,33,34,37,38,41,42,43,44,45,46,49,50,51,52,53,54,58,59,60,61}; 


#define sample_num 3500  // number of samples, positive+ negative
#define feature_num 1111 // number of features of LBF-HF descriptor, 30*37 + 1(label )
#define TRAIN 0          // 1 for training, 0 for prediction 
#define pos_num 500      // number of positive samples 
int hist[58]={0};
int total_num=0;
double sample[sample_num][feature_num] = {0.0}; // this array stores feature of all the samples



#define N	8				// length of the sequence 
#define PI	3.1415926535	
typedef double ElementType;
typedef struct				//complex structure for Fourier operator 
{
	ElementType real,imag;
}complex_lbp;
complex_lbp dataResult[N];		 // the value of the sequence in frequency domain
ElementType dataSource[N];	     // primitive input data sequence
ElementType dataFinualResult[N]; // final value of the sequence in frequency domain

void main()
{
	CvSVM svm;
	
	// parameters/hyperparameters of SVM classifier 
	CvSVMParams param;
	param.svm_type = CvSVM::C_SVC;
	param.kernel_type = CvSVM::POLY;
	param.degree = 4;
	param.gamma = 4;
	param.coef0 = 1;
	//param.term_crit=cvTermCriteria(CV_TERMCRIT_ITER,100,FLT_EPSILON);

	if(TRAIN)
	// train the classifier
	{
		CvMat *dataMat = cvCreateMat(sample_num, feature_num-1, CV_32FC1); // stores the feature of all the samples, for training SVM
		CvMat *labelMat = cvCreateMat(sample_num, 1, CV_32SC1);            // labels of the samples

		// set all the labes to -1
		for (int i=0;i<sample_num;i++) 
		{
			sample[i][0]=-1.0;
		}

		// set the lebels of the positive samples to 1
		for (int j=3000;j<3500;j++) 
		{
			sample[j][0]=1.0;
		}
	
		for (int pic_num=0;pic_num<sample_num;pic_num++)
		{
			// Path for loading an image
			char filename[50] = "D:\\train\\sample\\";
			char img_num[10];
			char file_type[] = ".jpg";
			sprintf(img_num, "%d", pic_num+1);
			strcat(img_num, file_type);
			strcat(filename, img_num); 

			IplImage* image = cvLoadImage(filename,0);
			if (image == NULL)
			{
				return;
			}

			for (int block_num_final = 0; block_num_final < 30; block_num_final++)
			{
				re_initial();
				LBP_HF(image,AU_feature[block_num_final]);
				for (int i=0;i<7;i++)
				{
					for (int j=1+i*8;j<9+i*8;j++)
					{
						dataSource[j-1-i*8]=hist[j]*1.0/total_num; //calculate the 8 uniform LBP in a row
					}
					FFT_Calculate();
					for(int k=0; k<5; k++)
					{
						sample[pic_num][1+block_num_final*37+i*5+k]=dataFinualResult[k];
					}
				}
				// normalization
				sample[pic_num][1+block_num_final*37+35]=hist[0]*1.0/total_num;
				sample[pic_num][1+block_num_final*37+36]=hist[57]*1.0/total_num;
			}
		}

		for (int i = 0; i<sample_num; i++)
		{
			for (int j = 1; j<feature_num; j++)
			{
				cvSetReal2D(dataMat, i, j-1, sample[i][j]);
			}
			cvSetReal2D(labelMat, i, 0, sample[i][0]);
		}

		// train the classifier and save it
		svm.train(dataMat, labelMat, NULL, NULL, param);
		svm.save("lbp_svm_val_surprise.txt");

		system("pause");
		cvReleaseMat(&dataMat);
		cvReleaseMat(&labelMat);
	}

	else
	// load the classifier and make predictions
	{
		svm.load("lbp_svm_val_surprise.txt");
		for(int pic_num=3000;pic_num<3500;pic_num++)
		{
			// Path for loading an image
			char filename[50] = "D:\\train\\surprise\\";
			char img_num[10];
			char file_type[] = ".jpg";
			sprintf(img_num, "%d", pic_num+1);
			strcat(img_num, file_type);
			strcat(filename, img_num); 
		
	    	IplImage *image2 = cvLoadImage(filename, 0);
		
			double final_vector[feature_num-1]={0.0}; 
			get_vector_AU(image2,final_vector);
			CvMat *testMat = cvCreateMat(1, feature_num-1, CV_32FC1);
		
			for (int i = 0; i<feature_num-1; i++)
			{
				cvSetReal2D(testMat, 0, i, final_vector[i]); 
			}

			// make prediction
			float flag = svm.predict(testMat);

			cout << flag << endl;
			
		}
		system("pause");
	}
}


void FFT_Calculate_OneNode(int k)
// calculate the DFT value of a point in the frequency domain
{
	int n = 0;
	complex_lbp ResultThisNode;
	complex_lbp part[N];
	ResultThisNode.real = 0;
	ResultThisNode.imag = 0;
	for(n=0; n<N; n++)
	{
		// real part and imaginary part(Eular equation) 
		part[n].real = cos(2*PI/N*k*n)*dataSource[n];
		part[n].imag = -sin(2*PI/N*k*n)*dataSource[n];

		ResultThisNode.real += part[n].real;
		ResultThisNode.imag += part[n].imag;
	}
	dataResult[k].real = ResultThisNode.real;
	dataResult[k].imag = ResultThisNode.imag;
}

void FFT_Calculate()
//calculate DFT for all the input sequence
{
	int i = 0;
	for(i=0; i<N; i++)
	{
		FFT_Calculate_OneNode(i);
		dataFinualResult[i] = sqrt(dataResult[i].real * dataResult[i].real + dataResult[i].imag * dataResult[i].imag);
	}
}

void re_initial()
{
	for (int j=0;j<58;j++)
	{
		hist[j]=0;
	}
	total_num=0;
	for (int i=0;i<N;i++)
	{
		dataResult[i].imag=0.0;	
		dataResult[i].real=0.0;	// final value of the sequence in frequency domain
		dataSource[i]=0.0;	    // primitive input data sequence
		dataFinualResult[i]=0.0;
	}

}

void LBP_HF(IplImage* image, int num)
// extract LBP-HF feature
{
	int width  = image->width;
	int height = image->height;

	int col_num=num%8;
	int row_num=num/8;

	for (int i = 1+height/8*row_num; i <height/8*(row_num+1)-1; i++)
		for (int j = 1+width/8*col_num; j <width/8*(col_num+1)-1; j++)
		{
			total_num++;
			int center = (unsigned char)image->imageData[i*width + j];
			unsigned char result = 0;
			int temp = 0;

			temp = (unsigned char)image->imageData[(i - 1)*width + j - 1];
			//if (abs(temp - center)>gap)
			if (temp>center)
			{
				result += 1;
			}
			temp = (unsigned char)image->imageData[(i - 1)*width + j];
			if (temp>center)
			{
				result += 2;
			}
			temp = (unsigned char)image->imageData[(i - 1)*width + j + 1];
			if (temp>center)
			{
				result += 4;
			}
			temp= (unsigned char)image->imageData[i*width + j + 1];
			if (temp>center)
			{
				result += 8;
			}
			temp = (unsigned char)image->imageData[(i+1)*width + j + 1];
			if (temp>center)
			{
				result += 16;
			}
			temp = (unsigned char)image->imageData[(i + 1)*width + j ];
			if (temp>center)
			{
				result += 32;
			}
			temp = (unsigned char)image->imageData[(i + 1)*width + j-1];
			if (temp>center)
			{
				result += 64;
			}
			temp = (unsigned char)image->imageData[i*width + j - 1];
			if (temp>center)
			{
				result += 128;
			}	

			if (result == 0)
			{
				hist[0]++;
			}
			else if (result == 255)
			{
				hist[57]++;
			}
			else
			{
				unsigned char temp = result;
				unsigned char img_data = result;
				unsigned char temp_1=result;
				if (result>=128)
				{
					result = result << 1;
					result +=1;
				}
				else
				{
					result = result << 1;
				}

				result = result^temp;

				int count = 0;
				int bit_count = 0;
				while (result > 0)
				{
					result = result&(result - 1);
					count++;
				}
				while (temp > 0)
				{
					temp = temp&(temp - 1);
					bit_count++;
				}
				if (count == 2)
				{
					for (int i=0;i<8;i++)
					{
						if (rotate_map[bit_count-1][i]==temp_1)
						{
							int map_index=(bit_count-1)*8+1+i;
							hist[map_index]++;
							break;
						}
					}
				}
			}
		}

}

void get_vector_AU(IplImage* image,double final_vector[])
// extract LBP-HF feature && normalization
{
	for (int block_num_final=0;block_num_final<30;block_num_final++)
	{
		re_initial();
		LBP_HF(image,AU_feature[block_num_final]);
		for (int i=0;i<7;i++)
		{
			for (int j=1+i*8;j<9+i*8;j++)
			{
				dataSource[j-1-i*8]=hist[j]*1.0/total_num; //calculate the 8 uniform LBP in a row
			}
			FFT_Calculate();
			for(int k=0; k<5; k++)
			{
				final_vector[block_num_final*37+i*5+k]=dataFinualResult[k];
			}
		}
		final_vector[block_num_final*37+35]=hist[0]*1.0/total_num;
		final_vector[block_num_final*37+36]=hist[57]*1.0/total_num;

	}
}
/*void LBP_HF(IplImage* image, int x, int y,int rows,int cols)
{
	int width  = image->width;
	int height = image->height;

	int x_start=x;
	int y_start=y-25;
	if (y_start<=1)
	{
		y_start=1;
	}

	for (int i = 1+y_start; i <y_start+rows-1; i++)
		for (int j = 1+x_start; j <x_start+cols-1; j++)
		{
			total_num++;
			int center = (unsigned char)image->imageData[i*width + j];
			unsigned char result = 0;
			int temp = 0;

			temp = (unsigned char)image->imageData[(i - 1)*width + j - 1];
			//if (abs(temp - center)>gap)
			if (temp>center)
			{
				result += 1;
			}
			temp = (unsigned char)image->imageData[(i - 1)*width + j];
			if (temp>center)
			{
				result += 2;
			}
			temp = (unsigned char)image->imageData[(i - 1)*width + j + 1];
			if (temp>center)
			{
				result += 4;
			}
			temp= (unsigned char)image->imageData[i*width + j + 1];
			if (temp>center)
			{
				result += 8;
			}
			temp = (unsigned char)image->imageData[(i+1)*width + j + 1];
			if (temp>center)
			{
				result += 16;
			}
			temp = (unsigned char)image->imageData[(i + 1)*width + j ];
			if (temp>center)
			{
				result += 32;
			}
			temp = (unsigned char)image->imageData[(i + 1)*width + j-1];
			if (temp>center)
			{
				result += 64;
			}
			temp = (unsigned char)image->imageData[i*width + j - 1];
			if (temp>center)
			{
				result += 128;
			}	

			if (result == 0)
			{
				hist[0]++;
			}
			else if (result == 255)
			{
				hist[57]++;
			}
			else
			{
				unsigned char temp = result;
				unsigned char img_data = result;
				unsigned char temp_1=result;
				if (result>=128)
				{
					result = result << 1;
					result +=1;
				}
				else
				{
					result = result << 1;
				}

				result = result^temp;

				int count = 0;
				int bit_count = 0;
				while (result > 0)
				{
					result = result&(result - 1);
					count++;
				}
				while (temp > 0)
				{
					temp = temp&(temp - 1);
					bit_count++;
				}
				if (count == 2)
				{
					for (int i=0;i<8;i++)
					{
						if (rotate_map[bit_count-1][i]==temp_1)
						{
							int map_index=(bit_count-1)*8+1+i;
							hist[map_index]++;
							break;
						}
					}
				}
			}
		}

}*/
