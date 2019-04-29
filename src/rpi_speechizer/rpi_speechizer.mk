##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=rpi_speechizer
ConfigurationName      :=Release
WorkspacePath          :=/home/serj/codez/work1
ProjectPath            :=/home/serj/codez/work1/rpi_speechizer
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=
Date                   :=28/02/19
CodeLitePath           :=/home/serj/.codelite
LinkerName             :=/opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-gcc
SharedObjectLinkerName :=/opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=$(PreprocessorSwitch)NDEBUG 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="rpi_speechizer.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -static -pthread
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)/home/serj/rpi/alsa/install/usr/include 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)asound 
ArLibs                 :=  "asound" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/home/serj/rpi/alsa/install/usr/lib 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-ar rcu
CXX      := /opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-g++
CC       := /opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-gcc
CXXFLAGS :=  -O2 -Wall $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /opt/arm-rpi-gcc-musl/bin/arm-rpi-linux-musleabihf-as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/main.c$(ObjectSuffix) $(IntermediateDirectory)/BCM2835gpio.c$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


$(IntermediateDirectory)/.d:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/main.c$(ObjectSuffix): main.c $(IntermediateDirectory)/main.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/serj/codez/work1/rpi_speechizer/main.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/main.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/main.c$(DependSuffix): main.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/main.c$(ObjectSuffix) -MF$(IntermediateDirectory)/main.c$(DependSuffix) -MM main.c

$(IntermediateDirectory)/main.c$(PreprocessSuffix): main.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/main.c$(PreprocessSuffix) main.c

$(IntermediateDirectory)/BCM2835gpio.c$(ObjectSuffix): BCM2835gpio.c $(IntermediateDirectory)/BCM2835gpio.c$(DependSuffix)
	$(CC) $(SourceSwitch) "/home/serj/codez/work1/rpi_speechizer/BCM2835gpio.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/BCM2835gpio.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/BCM2835gpio.c$(DependSuffix): BCM2835gpio.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/BCM2835gpio.c$(ObjectSuffix) -MF$(IntermediateDirectory)/BCM2835gpio.c$(DependSuffix) -MM BCM2835gpio.c

$(IntermediateDirectory)/BCM2835gpio.c$(PreprocessSuffix): BCM2835gpio.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/BCM2835gpio.c$(PreprocessSuffix) BCM2835gpio.c


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


