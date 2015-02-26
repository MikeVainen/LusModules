#include "itkImageFileWriter.h"

#include "itkSmoothingRecursiveGaussianImageFilter.h"

#include "itkPluginUtilities.h"

#include "LusSegmenterCLP.h"
#include "itkExtractImageFilter.h"

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <Windows.h>
#include <list>
#include "vnl/vnl_math.h"

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIterator.h"
#include "itkImageLinearIteratorWithIndex.h"

#include "itkGradientAnisotropicDiffusionImageFilter.h"
#include <itkCurvatureAnisotropicDiffusionImageFilter.h>

#include "itkBinaryThresholdImageFilter.h"
#include "itkThresholdImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include <itkGradientMagnitudeImageFilter.h>
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"

#include <itkGrayscaleErodeImageFilter.h>
#include <itkGrayscaleDilateImageFilter.h>

#include <itkVotingBinaryHoleFillingImageFilter.h>
#include <itkCastImageFilter.h>

#include "itkHoughTransform2DCirclesImageFilter.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkDiscreteGaussianImageFilter.h"

#include <itkBinaryCrossStructuringElement.h>
#include "itkFlatStructuringElement.h"
#include <itkBinaryBallStructuringElement.h>
#include "itkLineSpatialObject.h"
#include "itkLineSpatialObjectPoint.h"
#include "itkLineIterator.h"
#include "itkSurfaceSpatialObject.h"



using namespace std;


namespace
{

template <class T>


int DoIt( int argc, char * argv[], T )
{
  PARSE_ARGS;

  typedef          T           InputPixelType;
  typedef          T           OutputPixelType;
  typedef unsigned short       ImagePixelType;
  typedef          float       AnisotropicPixelType;
  typedef          float       AccumulatorPixelType;
  
  const   unsigned int         Dimension = 2;

  typedef itk::Image<InputPixelType, Dimension> InputImageType;
  typedef itk::Image<ImagePixelType, Dimension> ImageType;
  typedef itk::Image<OutputPixelType, Dimension> OutputImageType;
  typedef itk::Image<AnisotropicPixelType,Dimension> GradientImageType;

  typedef itk::ImageFileReader<InputImageType> ReaderType;
  typedef itk::ImageFileWriter<OutputImageType> WriterType;

  typedef itk::ImageLinearIteratorWithIndex<InputImageType> IteratorType;
  typedef itk::ImageLinearIteratorWithIndex<ImageType> MaskIteratorType;
  typedef itk::CurvatureAnisotropicDiffusionImageFilter<OutputImageType, GradientImageType> AnisoCurvatureType;

   //Better results in terms of smoothing are oftenly obtained by applying the Curvature version of the anisotropic filter

  typedef itk::VotingBinaryHoleFillingImageFilter<ImageType,ImageType >  FillingFilterType;
  typedef itk::FlatStructuringElement<Dimension> FlatStructuringElementType;
  typedef itk::BinaryCrossStructuringElement<ImagePixelType, Dimension> StructuringElementType;
  typedef itk::GrayscaleErodeImageFilter<ImageType,ImageType, FlatStructuringElementType> ErodeFilterType;
  typedef itk::GrayscaleDilateImageFilter<ImageType,ImageType, FlatStructuringElementType> DilateFilterType;

  typedef itk::CastImageFilter<GradientImageType,OutputImageType> CasterType;
  typedef itk::GradientMagnitudeImageFilter<ImageType,GradientImageType> GradMagType;
  typedef itk::GradientMagnitudeRecursiveGaussianImageFilter<ImageType,GradientImageType> RecursiveGradMadType;

  typedef itk::BinaryThresholdImageFilter <ImageType, ImageType> BinaryThresholdImageFilterType;
 
  BinaryThresholdImageFilterType::Pointer thresholdFilter = BinaryThresholdImageFilterType::New();

   
  

  ReaderType::Pointer reader = ReaderType::New();
  WriterType::Pointer writer = WriterType::New();
  FillingFilterType::Pointer filler = FillingFilterType::New();
  ErodeFilterType::Pointer eroder = ErodeFilterType::New();
  DilateFilterType::Pointer dilater = DilateFilterType::New();
  AnisoCurvatureType::Pointer aniCur = AnisoCurvatureType::New();
  CasterType::Pointer caster = CasterType::New();
  GradMagType::Pointer gradMagnitude = GradMagType::New();
  RecursiveGradMadType::Pointer recursiveGradMagnitude = RecursiveGradMadType::New();


  InputImageType::Pointer image;
  ImageType::Pointer maskImage = ImageType::New();
  ImageType::Pointer finalMaskImage = ImageType::New();
  ImageType::Pointer resultImage = ImageType::New();
  OutputImageType::Pointer outImage = OutputImageType::New();

  InputImageType::IndexType localIndex;
  ImageType::IndexType centerIndex;

  typedef itk::Image< AccumulatorPixelType, Dimension > AccumulatorImageType;
  typedef itk::LineSpatialObject< Dimension >   LineType;
  typedef itk::Point< double, ImageType::ImageDimension > PointType;

  typedef itk::SurfaceSpatialObject<Dimension> SurfaceType;
  typedef SurfaceType::Pointer SurfacePointer;
  typedef itk::SurfaceSpatialObjectPoint<Dimension> SurfacePointType;
  typedef itk::CovariantVector<double,Dimension> VectorType;
  SurfacePointer Surface = SurfaceType::New();
  SurfaceType::PointListType list;

  reader->SetFileName( inputImage.c_str() );
   try
    {
		reader->Update();
		image = reader->GetOutput();
    }
   catch ( itk::ExceptionObject &err)
    {
		 std::cout << "ExceptionObject caught a !" << std::endl;
		std::cout << err << std::endl;
		return -1;
    }


 //We create a mask and output image of the same size as the one we're setting in

  ImageType::RegionType region = image->GetLargestPossibleRegion();

  // Pixel data is allocated
  maskImage->SetRegions( region );
  maskImage->Allocate();
  finalMaskImage->SetRegions( region );
  finalMaskImage->Allocate();
  resultImage->SetRegions( region );
  resultImage->Allocate();
  outImage->SetRegions( region );
  outImage->Allocate();


  itk::ImageRegionIterator<ImageType> imageIterator(maskImage,maskImage->GetLargestPossibleRegion());
 
  // Make the black base for the mask image
  while(!imageIterator.IsAtEnd())
    {
     imageIterator.Set(0);
	 finalMaskImage->SetPixel(imageIterator.GetIndex(),0);
	 resultImage->SetPixel(imageIterator.GetIndex(),0);
	 maskImage->SetPixel(imageIterator.GetIndex(),0);
	 outImage->SetPixel(imageIterator.GetIndex(),0);
     ++imageIterator;
    }

   OutputImageType::IndexType start;
  start[0] = bX ;
  start[1] = bY ;
  // Software Guide : EndCodeSnippet


  // Software Guide : BeginCodeSnippet
  OutputImageType::SizeType size;
  size[0] = region.GetSize()[0] - bX ;
  size[1] = eY-bY;
  // Software Guide : EndCodeSnippet


  InputImageType::RegionType desiredInputRegion;
  desiredInputRegion.SetSize(  size  );
  desiredInputRegion.SetIndex( start );

  ImageType::RegionType desiredMaskRegion;
  desiredMaskRegion.SetSize(  size  );
  desiredMaskRegion.SetIndex( start );


  // Software Guide : BeginCodeSnippet
  ImageType::SizeType indexRadius;

  indexRadius[0] = radius; // radius along x
  indexRadius[1] = radius; // radius along y

  filler->SetRadius( indexRadius );
  // Software Guide : EndCodeSnippet


  // Software Guide : BeginCodeSnippet
  filler->SetBackgroundValue(   0 );
  filler->SetForegroundValue( 255 );
  // Software Guide : EndCodeSnippet

  // Software Guide : BeginCodeSnippet
  filler->SetMajorityThreshold( 2 );
  // Software Guide : EndCodeSnippet
  
   StructuringElementType  structuringElement;
  structuringElement.SetRadius(radius-4);
  structuringElement.CreateStructuringElement();


  int i=0, j=0, NUM= radius-4; 
  FlatStructuringElementType::RadiusType rad; 
  rad.Fill(NUM);

  //we design the int matrix that will be used to create de Structuring Element
    vector<vector<int>> diamondStructure(NUM+NUM+1, vector<int>(NUM+NUM+1));
 
  for(i=-NUM; i<=NUM; i++){
	  for(j=-NUM; j<=NUM; j++){
		  if( abs(i)+abs(j)<=NUM){
			 diamondStructure[NUM+i][NUM+j] = 1;
		  }
          else { 
			 diamondStructure[NUM+i][NUM+j] = 0;
		  }
		}
	}

  FlatStructuringElementType elementoEstructurante = FlatStructuringElementType::Box(rad);  

  i=0;
  j=0;

  //Now we introduce the elements into the element with that exact diamond structure
  for(FlatStructuringElementType::Iterator iter = 
elementoEstructurante.Begin(); iter != elementoEstructurante.End(); ++iter, j++) 
  { 
		if(j==(2*NUM+1)){
			j=0;
			++i;
			std::cout << endl ;
		}
		*iter = diamondStructure[i][j];
		//we print the look of the structuring element
		std::cout << *iter;
  } 
  std::cout<<endl;

  
  eroder->SetKernel(elementoEstructurante);
  dilater->SetKernel(elementoEstructurante);

  thresholdFilter->SetLowerThreshold(lowerthreshold);
  thresholdFilter->SetUpperThreshold(upperthreshold);
  thresholdFilter->SetInsideValue(255);
  thresholdFilter->SetOutsideValue(0);

  IteratorType inputIt( image, desiredInputRegion ); 

  inputIt.SetDirection(1);

  MaskIteratorType maskIt( finalMaskImage, desiredInputRegion);
  maskIt.SetDirection(1);

  for ( maskIt.GoToBegin(), inputIt.GoToBegin() ; !inputIt.IsAtEnd();  ++maskIt, ++inputIt )
    {
		maskIt.Set(inputIt.Get()); 
		if(inputIt.IsAtEndOfLine()){
			inputIt.NextLine();
			maskIt.NextLine();
		}
   }
  
  thresholdFilter->SetInput(finalMaskImage);
  filler->SetInput(thresholdFilter->GetOutput());
  eroder->SetInput(filler->GetOutput());
  dilater->SetInput(eroder->GetOutput());
  finalMaskImage =  dilater->GetOutput() ;
  finalMaskImage->Update();


  typedef itk::HoughTransform2DCirclesImageFilter<AnisotropicPixelType,AccumulatorPixelType> HoughTransformFilterType;
  HoughTransformFilterType::Pointer houghFilter= HoughTransformFilterType::New();
  GradMagType::Pointer gradFilter = GradMagType::New();

  gradFilter->SetInput(finalMaskImage);
  houghFilter->SetInput(gradFilter->GetOutput());

  houghFilter->SetNumberOfCircles( ncircles );
  houghFilter->SetMinimumRadius(   minrad );
  houghFilter->SetMaximumRadius(   maxrad );

  houghFilter->Update();
  AccumulatorImageType::Pointer localAccumulator = houghFilter->GetOutput();

  HoughTransformFilterType::CirclesListType circles;
  circles = houghFilter->GetCircles( ncircles );

  

  typedef HoughTransformFilterType::CirclesListType CirclesListType;
  CirclesListType::const_iterator itCircles = circles.begin();

  
  int Xsize = region.GetSize()[0];
  InputImageType::IndexType actual;
  actual[1]=0;
  int* contour1 = new int[Xsize];
  int* contour2 = new int[Xsize];
  int* finalcontour = new int[Xsize];

  std::vector<ImageType::IndexType> origins (ncircles); 
  int numCircles = 0;
  int vlimit = 0;

  while( itCircles != circles.end() )
    {
		std::cout << "Center: ";
		std::cout << (*itCircles)->GetObjectToParentTransform()->GetOffset()<< std::endl;
		std::cout << "Radius: " << (*itCircles)->GetRadius()[0] << std::endl;
		centerIndex[0] = (short int)((*itCircles)->GetObjectToParentTransform()->GetOffset()[0]);
		centerIndex[1] = (short int)((*itCircles)->GetObjectToParentTransform()->GetOffset()[1]);
        
		if(finalMaskImage->GetPixel(centerIndex)>0 && (*itCircles)->GetRadius()[0]>minrad ){
			 finalMaskImage->SetPixel(centerIndex, 90);
			 if(centerIndex[1]>vlimit) vlimit = centerIndex[1];
			 ++numCircles;
			 
	    }

	    
	    itCircles++;
	    
    

	}

  InputImageType::IndexType cstart;
	cstart[0] = bX;
	cstart[1] = bY;

  InputImageType::SizeType csize;
	csize[0] = region.GetSize()[0] - bX;
	csize[1] = 140;

	
	InputImageType::RegionType contourRegion;
	contourRegion.SetSize(  csize  );
	contourRegion.SetIndex( cstart );

	if ( numCircles> ncircles/2.5){
		std::cout << "Detectado contorno" << std::endl;
		csize[1] = vlimit - cstart[1] + 7;
		contourRegion.SetSize(  csize  );
		itk::ImageIOBase::IOPixelType     pType;
		itk::ImageIOBase::IOComponentType cType;

  
		itk::GetImageType(inputImage, pType, cType);

		switch( cType )
		{
			case itk::ImageIOBase::UCHAR:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<unsigned char>(0));
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::CHAR:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<char>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::USHORT:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<unsigned short>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::SHORT:
				 lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<short>(0) );
				 dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				 break;
			case itk::ImageIOBase::UINT:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<unsigned int>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::INT:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<int>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::ULONG:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<unsigned long>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::LONG:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<long>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::FLOAT:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<float>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				 break;
			case itk::ImageIOBase::DOUBLE:
				lus::getImagesRegionOfInterest(argc,argv,maskImage,contourRegion,120,static_cast<double>(0) );
				dilater->SetInput(maskImage);
				eroder->SetInput(dilater->GetOutput());
				maskImage = eroder->GetOutput();
				maskImage->Update();
				break;
			case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
			default:
				std::cout << "unknown component type" << std::endl;
				break;
		}

		itk::ImageRegionIterator<OutputImageType> outIterator(outImage,contourRegion);
		itk::ImageRegionIterator<ImageType> resultIterator(maskImage,contourRegion);
		outIterator.GoToBegin();
		resultIterator.GoToBegin();
		while(!resultIterator.IsAtEnd())
		{
			if(resultIterator.Get()!=0) outIterator.Set(image->GetPixel(resultIterator.GetIndex()));
			++outIterator;
			++resultIterator;
			
			
		}
	}

	else{
		itk::ImageRegionIterator<InputImageType> inIterator(image,contourRegion);
		itk::ImageRegionIterator<ImageType> resultIterator(resultImage,contourRegion);
		inIterator.GoToBegin();
		resultIterator.GoToBegin();
		while(!inIterator.IsAtEnd())
		{
			if(inIterator.Get()>45)resultIterator.Set(255);
			++inIterator;
			++resultIterator;
			
			
		}
		filler->SetInput(resultImage);
		eroder->SetInput(filler->GetOutput());
		dilater->SetInput(eroder->GetOutput());
		maskImage = dilater->GetOutput();
		maskImage->Update();
		lus::nonContourDetectedObtainMask(maskImage, 150);
		filler->SetInput(maskImage);
		eroder->SetInput(filler->GetOutput());
		maskImage = eroder->GetOutput();
		maskImage->Update();
		itk::ImageRegionIterator<ImageType> result2Iterator(maskImage,contourRegion);
		itk::ImageRegionIterator<OutputImageType> outIterator(outImage,contourRegion);
		outIterator.GoToBegin();
		result2Iterator.GoToBegin();
		while(!outIterator.IsAtEnd())
		{
			if(result2Iterator.Get()!=0) outIterator.Set(image->GetPixel(result2Iterator.GetIndex()));
			++outIterator;
			++result2Iterator;
			
			
		}

	}


	
	writer->SetInput(outImage);



	
	
  



	writer->SetFileName( outputImage.c_str() );
  
  //  Software Guide : BeginLatex
  //
  //  Image types are defined below.
  //
  //  Software Guide : EndLatex


  
  // Software Guide : BeginCodeSnippet

  try
    {	
		writer->Update();

    }

  catch( itk::ExceptionObject & err )
    {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << err << std::endl;
    return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;

}
// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
}
  int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;

  try
    {
    itk::GetImageType(inputImage, pixelType, componentType);

    // This filter handles all types on input, but only produces
    // signed types
    switch( componentType )
      {
      case itk::ImageIOBase::UCHAR:
        return DoIt( argc, argv, static_cast<unsigned char>(0) );
        break;
      case itk::ImageIOBase::CHAR:
        return DoIt( argc, argv, static_cast<char>(0) );
        break;
      case itk::ImageIOBase::USHORT:
        return DoIt( argc, argv, static_cast<unsigned short>(0) );
        break;
      case itk::ImageIOBase::SHORT:
        return DoIt( argc, argv, static_cast<short>(0) );
        break;
      case itk::ImageIOBase::UINT:
        return DoIt( argc, argv, static_cast<unsigned int>(0) );
        break;
      case itk::ImageIOBase::INT:
        return DoIt( argc, argv, static_cast<int>(0) );
        break;
      case itk::ImageIOBase::ULONG:
        return DoIt( argc, argv, static_cast<unsigned long>(0) );
        break;
      case itk::ImageIOBase::LONG:
        return DoIt( argc, argv, static_cast<long>(0) );
        break;
      case itk::ImageIOBase::FLOAT:
        return DoIt( argc, argv, static_cast<float>(0) );
        break;
      case itk::ImageIOBase::DOUBLE:
        return DoIt( argc, argv, static_cast<double>(0) );
        break;
      case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
      default:
        std::cout << "unknown component type" << std::endl;
        break;
      }
    }

  catch( itk::ExceptionObject & excep )
    {
    std::cerr << argv[0] << ": exception caught !" << std::endl;
    std::cerr << excep << std::endl;
    return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}


namespace lus{

	template <class L>

	int applyMask(int argc, char * argv[], itk::Image<unsigned short,2>::Pointer mImage, itk::Image<unsigned short,2>::Pointer rImage, L){

		PARSE_ARGS;

		typedef          L           InputPixelType;
		typedef          L           OutputPixelType;
  
		typedef itk::ImageFileReader<itk::Image<InputPixelType,2>> ReaderType;
		typedef itk::ImageLinearIteratorWithIndex<itk::Image<unsigned short,2>> contourIterator;
		typedef itk::ImageLinearIteratorWithIndex<itk::Image<InputPixelType,2>> inputIterator;

		ReaderType::Pointer inputReader = ReaderType::New();
		itk::Image<InputPixelType,2>::Pointer readImage;

		inputReader->SetFileName( inputImage.c_str() );
		try
		{
			inputReader->Update();
			readImage = inputReader->GetOutput();
		}
		catch ( itk::ExceptionObject &err)
		{
			 std::cout << "ExceptionObject caught a !" << std::endl;
			std::cout << err << std::endl;
			return -1;
		}

	
		inputIterator iIt(readImage, readImage->GetRequestedRegion());
		iIt.GoToBegin();

		contourIterator mIt(mImage, mImage->GetRequestedRegion());
		mIt.GoToBegin();

		contourIterator rIt(rImage, rImage->GetRequestedRegion());
		rIt.GoToBegin();

		for ( iIt.GoToBegin(),  mIt.GoToBegin(),  rIt.GoToBegin(); ! iIt.IsAtEnd(); ++iIt, ++mIt, ++rIt )
		{
			
			if(mIt.Get()!=0) rIt.Set(iIt.Get());
			
		}

	
		return 0;

	}
	
	template <class J>

	int getImagesRegionOfInterest(int argc, char * argv[], itk::Image<unsigned short,2>::Pointer mask, itk::ImageRegion<2> region, int threshold, J){

		PARSE_ARGS;

		typedef          J           InputPixelType;
		typedef          J           OutputPixelType;
  
		typedef itk::ImageFileReader<itk::Image<InputPixelType,2>> ReaderType;
		typedef itk::ImageLinearIteratorWithIndex<itk::Image<unsigned short,2>> contourIterator;
		typedef itk::ImageLinearIteratorWithIndex<itk::Image<InputPixelType,2>> inputIterator;

		ReaderType::Pointer inputReader = ReaderType::New();
		itk::Image<unsigned short,2>::IndexType actual;
		itk::Image<InputPixelType,2>::Pointer readImage;

		inputReader->SetFileName( inputImage.c_str() );
		try
		{
			inputReader->Update();
			readImage = inputReader->GetOutput();
		}
		catch ( itk::ExceptionObject &err)
		{
			 std::cout << "ExceptionObject caught a !" << std::endl;
			std::cout << err << std::endl;
			return -1;
		}

		inputIterator aIt(readImage,region);
		contourIterator bIt(mask,region);

		aIt.SetDirection(1);
		bIt.SetDirection(1);



		for ( aIt.GoToBegin(),  bIt.GoToBegin(); ! aIt.IsAtEnd(); ++aIt, ++bIt )
		{

			if (aIt.Get() >= threshold){
				actual=bIt.GetIndex();
			}

			if(aIt.IsAtEndOfLine()){
		
				//whenever the limit is detected we still give some space to ensure keeping the complete contour
				//actual[1]= actual[1]+3;
				for(bIt.GoToBeginOfLine(); ! bIt.IsAtEndOfLine() ;++bIt){
					if( actual[1]!= region.GetIndex()[1])bIt.Set(255);
					if( bIt.GetIndex()[1]==actual[1]+3){
						bIt.GoToEndOfLine();
						//bIt.Set(255);
					
					}
				}
				aIt.NextLine();
				bIt.NextLine();
				actual=aIt.GetIndex();
			}
		

		}

		return 0;


	}



	int nonContourDetectedObtainMask(itk::Image<unsigned short,2>::Pointer mask, int length){

		typedef itk::Image<unsigned short,2> maskImageType;
		typedef itk::ImageLinearIteratorWithIndex<maskImageType> maskImageIterator;

		maskImageType::RegionType desiredRegion = mask->GetLargestPossibleRegion();
		int width = desiredRegion.GetSize()[0];

		maskImageType::IndexType start;
		start[0] = 0 ;
		start[1] = 0 ;

		maskImageType::SizeType size;
		size[0] = width ;
		size[1] = length;

		maskImageType::RegionType maskRegion;
		maskRegion.SetSize(  size  );
		maskRegion.SetIndex( start );

		int* limits = new int[width];


		maskImageIterator maskImageIt(mask,maskRegion);
		maskImageIt.SetDirection(1);
		
		int towhere=0;
		int count=0;
		int column=0;

		for(maskImageIt.GoToBegin() ;!maskImageIt.IsAtEnd() ; ++maskImageIt ){
			if (maskImageIt.Get()!=0){
				towhere=maskImageIt.GetIndex()[1];
				maskImageIt.Set(0);
			}

			if(maskImageIt.IsAtEndOfLine()){
				limits[column]=towhere;
				++column;
				++count;
				if(count==15){
					towhere=0;
					count=0;
					for(int x=0; x<15; x++){
						if( limits[column - x] >= towhere) towhere = limits[column - x];
					}
					for(int y=0; y<15; y++){
						limits[column - y] = towhere;
					}
				}

				maskImageIt.NextLine();
				towhere=0;
			}
		}


		column = 0;
		maskImageIterator maskFillerIt(mask,maskRegion);
		maskFillerIt.SetDirection(1);

		for(maskFillerIt.GoToBegin() ;!maskFillerIt.IsAtEnd() ; ++maskFillerIt ){
			maskFillerIt.Set(255);
			if (maskFillerIt.GetIndex()[1]== limits[column]){
				maskFillerIt.NextLine();
				++column;
			}
			if (maskFillerIt.IsAtEndOfLine()){
				maskFillerIt.NextLine();
				++column;
			}

		}

		return 0;

	}


  }
