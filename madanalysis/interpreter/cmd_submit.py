################################################################################
#  
#  Copyright (C) 2012-2016 Eric Conte, Benjamin Fuks
#  The MadAnalysis development team, email: <ma5team@iphc.cnrs.fr>
#  
#  This file is part of MadAnalysis 5.
#  Official website: <https://launchpad.net/madanalysis5>
#  
#  MadAnalysis 5 is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#  
#  MadAnalysis 5 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with MadAnalysis 5. If not, see <http://www.gnu.org/licenses/>
#  
################################################################################


from madanalysis.interpreter.cmd_base                           import CmdBase
from madanalysis.IOinterface.job_writer                         import JobWriter
from madanalysis.IOinterface.layout_writer                      import LayoutWriter
from madanalysis.IOinterface.job_reader                         import JobReader
from madanalysis.IOinterface.folder_writer                      import FolderWriter
from madanalysis.enumeration.report_format_type                 import ReportFormatType
from madanalysis.layout.layout                                  import Layout
from madanalysis.install.install_manager                        import InstallManager
from madanalysis.configuration.delphesMA5tune_configuration     import DelphesMA5tuneConfiguration
from madanalysis.configuration.delphes_configuration            import DelphesConfiguration
from string_tools  import StringTools
import logging
import glob
import os
import time
import commands
import shutil

class CmdSubmit(CmdBase):
    """Command SUBMIT"""

    def __init__(self,main,resubmit=False):
        self.resubmit=resubmit
        if not resubmit:
            CmdBase.__init__(self,main,"submit")
        else:
            CmdBase.__init__(self,main,"resubmit")
        self.forbiddenpaths=[]
        self.forbiddenpaths.append(os.path.normpath(self.main.archi_info.ma5dir+'/lib'))
        self.forbiddenpaths.append(os.path.normpath(self.main.archi_info.ma5dir+'/bin'))
        self.forbiddenpaths.append(os.path.normpath(self.main.archi_info.ma5dir+'/madanalysis'))

    @staticmethod
    def chronometer_display(diff):
        fill=False
        theLast=time.localtime(diff)
        theLastStr=""
        if theLast.tm_mday>1:
            theLastStr=time.strftime('%d days ',theLast)
            fill=True
        elif theLast.tm_mday==0:
            theLastStr=time.strftime('%d day ',theLast)
        if theLast.tm_hour>1:
            theLastStr=time.strftime('%H hours ',theLast)
            fill=True
        elif theLast.tm_hour==0 or fill:
            theLastStr=time.strftime('%H hour ',theLast)
        if theLast.tm_min>1:
            theLastStr=time.strftime('%M minutes ',theLast)
            fill=True
        elif theLast.tm_min==0 or fill:
            theLastStr=time.strftime('%M minute ',theLast)
        if theLast.tm_sec>1:
            theLastStr=time.strftime('%s seconds ',theLast)
        else:
            theLastStr=time.strftime('%s second ',theLast)
        return theLastStr    
       

    def do(self,args,history):
        if not self.resubmit:
            return self.do_submit(args,history)
        else:
            return self.do_resubmit(args,history)


    def do_resubmit(self,args,history):

        # Start time
        start_time=time.time()

        # Checking argument number
        if len(args)!=0:
            self.logger.warning("Command 'resubmit' takes no argument. Any argument will be skipped.")

        # Checking presence of a valid job
        if self.main.lastjob_name is "":
            self.logger.error("an analysis must be defined and ran before using the resubmit command.") 
            return False

        self.main.lastjob_status = False

        # Checking if new plots or cuts have been performed
        ToReAnalyze = False
        
        # Look for the last submit and resubmit
        last_submit_cmd = -1
        for i in range(len(history)-1): # Last history entry should be resubmit
            if history[i].startswith('submit') or history[i].startswith('resubmit'):
                last_submit_cmd = i

        newhistory = []
        if last_submit_cmd==-1:
            ToReAnalyze = True
        else:
            for i in range(last_submit_cmd+1,len(history)):
                newhistory.append(history[i])

        ReAnalyzeCmdList = ['plot','select','reject','set main.clustering',
                            'set main.merging', 'define', 'set main.recast',
                            'import', 'set main.isolation']

        # Determining if we have to resubmit the job
        for cmd in newhistory:
            
            # Split cmd line into words
            words = cmd.split()
           
            # Creating a line with one whitespace between each word
            cmd2 = ''
            for word in words:
                if word!='':
                    cmd2+=word+' '
 
            # Looping over patterns
            for pattern in ReAnalyzeCmdList:
                if cmd2.startswith(pattern): 
                    ToReAnalyze = True
                    break

            # Found?
            if ToReAnalyze:
                break

        if ToReAnalyze:
            self.logger.info("   Creating the new histograms and/or applying the new cuts...")
            # Submission
            if not self.submit(self.main.lastjob_name,history):
                return
            self.logger.info("   Updating the reports...")
        else:
            self.logger.info("   No new histogram / cut to account for. Updating the reports...")

        # Reading info from job output
        layout = Layout(self.main)
        if not self.extract(self.main.lastjob_name,layout):
            return

        # Status = GOOD
        self.main.lastjob_status = True
        # Computing
        layout.Initialize()

        # Cleaning the directories
        if not FolderWriter.RemoveDirectory(self.main.lastjob_name+'/HTML',False):
            return
        if self.main.session_info.has_pdflatex:
            if not FolderWriter.RemoveDirectory(self.main.lastjob_name+'/PDF',False):
                return
        if self.main.session_info.has_latex:
            if not FolderWriter.RemoveDirectory(self.main.lastjob_name+'/DVI',False):
                return 

        # Creating the reports
        self.CreateReports([self.main.lastjob_name],history,layout)

        # End time 
        end_time=time.time()
           
        self.logger.info("   Well done! Elapsed time = " + CmdSubmit.chronometer_display(end_time-start_time) )

        
    def do_submit(self,args,history):

        # Start time
        start_time=time.time()

        # No arguments
        if len(args)==0:
            dirlist = os.listdir(self.main.currentdir)
            ii = 0
            while ('ANALYSIS_'+str(ii) in dirlist):
                ii = ii+1
            args.append(self.main.currentdir+'/ANALYSIS_'+str(ii))

        # Checking argument number
        if len(args)>1:
             self.logger.error("wrong number of arguments for the command 'submit'.")
             self.help()
             return

        # Checking if a dataset has been defined
        if len(self.main.datasets)==0:
            self.logger.error("no dataset found; please define a dataset (via the command import).")
            self.logger.error("job submission aborted.")
            return

        # Checking if a selection item has been defined
#        if len(self.main.selection)==0 and self.main.merging.enable==False and \
#               self.main.output == "":
#            self.logger.error("no analysis found. Please define an analysis (via the command plot).")
#            self.logger.error("job submission aborted.")
#            return

        # Treat the filename
        filename = os.path.expanduser(args[0])
        if not filename.startswith('/'):
            filename = self.main.currentdir + "/" + filename
        filename = os.path.normpath(filename)

        # Checking folder
        if filename in self.forbiddenpaths:
            self.logger.error("the folder '"+filename+"' is a MadAnalysis folder. " + \
                         "You cannot overwrite it. Please choose another folder.")
            return

        # Saving job name as global variable
        self.main.lastjob_name = filename
        self.main.lastjob_status = False

        # Submission
        self.logger.debug('Launching SampleAnalyzer ...')
        if not self.submit(filename,history):
            return

        # Reading info from job output
        self.logger.debug('Go back to the Python interface ...')
        layout = Layout(self.main)
        if not self.extract(filename,layout):
            return

        # Status = GOOD
        self.main.lastjob_status = True

        # Computing
        self.logger.info("   Preparing data for the reports ...")
        layout.Initialize()

        # Creating the reports
        if not self.main.recasting.status=="on":
            self.CreateReports(args,history,layout)

        # End time 
        end_time=time.time()
           
        self.logger.info("   Well done! Elapsed time = " + CmdSubmit.chronometer_display(end_time-start_time) )


    # Generating the reports
    def CreateReports(self,args,history,layout):

        output_paths = []
        modes        = []

        # Getting output filename for histo folder
        histopath = os.path.expanduser(args[0]+'/Histo')
        if not histopath.startswith('/'):
            histopath = self.main.currentdir + "/" + histopath
        histopath = os.path.normpath(histopath)

        # Getting output filename for HTML report
        htmlpath = os.path.expanduser(args[0]+'/HTML')
        if not htmlpath.startswith('/'):
            htmlpath = self.main.currentdir + "/" + htmlpath
        htmlpath = os.path.normpath(htmlpath)
        output_paths.append(htmlpath)
        modes.append(ReportFormatType.HTML)

        # Getting output filename for PDF report
        if self.main.session_info.has_pdflatex:
            pdfpath = os.path.expanduser(args[0]+'/PDF')
            if not pdfpath.startswith('/'):
                pdfpath = self.main.currentdir + "/" + pdfpath
            pdfpath = os.path.normpath(pdfpath)
            output_paths.append(pdfpath)
            modes.append(ReportFormatType.PDFLATEX)

        # Getting output filename for DVI report
        if self.main.session_info.has_latex:
            dvipath = os.path.expanduser(args[0]+'/DVI')
            if not dvipath.startswith('/'):
                dvipath = self.main.currentdir + "/" + dvipath
            dvipath = os.path.normpath(dvipath)
            output_paths.append(dvipath)
            modes.append(ReportFormatType.LATEX)

        # Creating folders
        if not layout.CreateFolders(histopath,output_paths,modes):
            return

        # Draw plots
        self.logger.info("   Generating all plots ...")
        if not layout.DoPlots(histopath,modes,output_paths):
            return
        
        # Generating the HTML report
        self.logger.info("   Generating the HMTL report ...")
        layout.GenerateReport(history,htmlpath,ReportFormatType.HTML)
        self.logger.info("     -> To open this HTML report, please type 'open'.")

        # PDF report
        if self.main.session_info.has_pdflatex:

            # Generating the PDF report
            self.logger.info("   Generating the PDF report ...")
            layout.GenerateReport(history,pdfpath,ReportFormatType.PDFLATEX)
            layout.CompileReport(ReportFormatType.PDFLATEX,pdfpath)

            # Displaying message for opening PDF
            if self.main.currentdir in pdfpath:
                pdfpath = pdfpath[len(self.main.currentdir):]
            if pdfpath[0]=='/':
                pdfpath=pdfpath[1:]
            self.logger.info("     -> To open this PDF report, please type 'open " + pdfpath + "'.")
            
        else:
            self.logger.warning("pdflatex not installed -> no PDF report.")

        # DVI/PDF report
        if self.main.session_info.has_latex:

            # Warning message for DVI -> PDF
            self.logger.info("   Generating the DVI report ...")
#            if not self.main.session_info.has_dvipdf:
#               self.logger.warning("dvipdf not installed -> the DVI report will not be converted to a PDF file.")

            # Generating the DVI report
            layout.GenerateReport(history,dvipath,ReportFormatType.LATEX)
            layout.CompileReport(ReportFormatType.LATEX,dvipath)

            # Displaying message for opening DVI
            if self.main.session_info.has_dvipdf:
                pdfpath = os.path.expanduser(args[0]+'/DVI')
                if self.main.currentdir in pdfpath:
                    pdfpath = pdfpath[len(self.main.currentdir):]
                if pdfpath[0]=='/':
                    pdfpath=pdfpath[1:]
                self.logger.info("     -> To open the corresponding Latex file, please type 'open " + pdfpath + "'.")
                
        else:
            self.logger.warning("latex not installed -> no DVI/PDF report.")



    def editDelphesCard(self,dirname):
        if self.main.forced or self.main.script:
            return

        self.logger.info("Would you like to edit the Delphes Card ? (Y/N)")
        allowed_answers=['n','no','y','yes']
        answer=""
        while answer not in  allowed_answers:
            answer=raw_input("Answer: ")
            answer=answer.lower()
        if answer=="no" or answer=="n":
            return
        else:
            if self.main.fastsim.package=="delphes":
                cardname = self.main.fastsim.delphes.card
            elif self.main.fastsim.package=="delphesMA5tune":
                cardname = self.main.fastsim.delphesMA5tune.card
            os.system(self.main.session_info.editor+" "+dirname+"/Input/"+cardname)

    def editRecastingCard(self,dirname):
        if self.main.forced or self.main.script:
            return

        self.logger.info("Would you like to edit the recasting Card ? (Y/N)")
        allowed_answers=['n','no','y','yes']
        answer=""
        while answer not in  allowed_answers:
            answer=raw_input("Answer: ")
            answer=answer.lower()
        if answer=="no" or answer=="n":
            return
        else:
            os.system(self.main.session_info.editor+" "+dirname+"/Input/recasting_card.dat")

    def submit(self,dirname,history):
        # checking if the needed version of delphes is activated
        forced_bkp = self.main.forced
        self.main.forced=True
        if self.main.fastsim.package == 'delphes' and not self.main.archi_info.has_delphes:
            installer=InstallManager(self.main)
            if self.main.archi_info.has_delphesMA5tune:
                if not installer.Deactivate('delphesMA5tune'):
                    return False
            if installer.Activate('delphes')==-1:
                return False
        if self.main.fastsim.package == 'delphesMA5tune' and not self.main.archi_info.has_delphesMA5tune:
            installer=InstallManager(self.main)
            if self.main.archi_info.has_delphes:
                if not installer.Deactivate('delphes'):
                    return False
            if installer.Activate('delphesMA5tune')==-1:
                return False
        self.main.forced=forced_bkp

        # Initializing the JobWriter
        jobber = JobWriter(self.main,dirname,self.resubmit)

        # Writing process
        if not self.resubmit:
            self.logger.info("   Creating folder '"+dirname.split('/')[-1] \
                +"'...")
        else:
            self.logger.info("   Checking the structure of the folder '"+\
               dirname.split('/')[-1]+"'...")
        if not jobber.Open():
            self.logger.error("job submission aborted.")
            return False

        if not self.resubmit:
            if self.main.recasting.status != 'on':
                self.logger.info("   Copying 'SampleAnalyzer' source files...")
            if not jobber.CopyLHEAnalysis():
                self.logger.error("   job submission aborted.")
                return False
            if self.main.recasting.status != 'on' and not jobber.CreateBldDir():
                self.logger.error("   job submission aborted.")
                return False
            if self.main.recasting.status == 'on':
                if not FolderWriter.CreateDirectory(dirname+'/Events'):
                    return False

        # In the case of recasting, there is no need to create a standard Job
        if self.main.recasting.status == "on":

            ### First, the analyses to take care off
            self.editRecastingCard(dirname)
            self.logger.info("   Getting the list of delphes simulation to be performed...")

            ### Second, which delphes run must be performed, and running them
            if not self.main.recasting.GetDelphesRuns(dirname+"/Input/recasting_card.dat"):
                return False
            firstv11 = True
            firstv13 = True
            forced_bkp = self.main.forced
            self.main.forced=True
            if len(self.main.recasting.delphesruns)==0:
                self.logger.warning('No recasting to do... Please check the recasting card')
                return False
            for mydelphescard in sorted(self.main.recasting.delphesruns):
                version=mydelphescard[:4]
                card=mydelphescard[5:]
                if version=="v1.1":
                    if not self.main.recasting.ma5tune:
                        self.logger.error('The DelphesMA5tune library is not present... v1.1 analyses cannot be used')
                        return False

                    if firstv11:
                        self.logger.info("   **********************************************************")
                        self.logger.info("   "+StringTools.Center('v1.1 detector simulations',57))
                        self.logger.info("   **********************************************************")
                        firstv11=False

                    ## Deactivating delphes
                    installer=InstallManager(self.main)
                    if not installer.Deactivate('delphes'):
                        return False

                    ## Activating and compile the MA5Tune
                    if installer.Activate('delphesMA5tune')==-1:
                        return False

                    ## running delphesMA5tune
                    for item in self.main.datasets:
                        if not os.path.isfile(dirname + '/Events/' + item.name + '_v1x1_' + \
                             card.replace('.tcl','')+'.root'):
                            self.main.recasting.status="off"
                            self.main.fastsim.package="delphesMA5tune"
                            self.main.fastsim.clustering=0
                            self.main.fastsim.delphes=0
                            self.main.fastsim.delphesMA5tune = DelphesMA5tuneConfiguration()
                            self.main.fastsim.delphesMA5tune.card = os.path.normpath("../../../../PADForMA5tune/Input/Cards/"+card)
                            self.submit(dirname+'_DelphesForMa5tuneRun',[])
                            self.main.recasting.status="on"
                            self.main.fastsim.package="none"
                            ## saving the output
                            shutil.move(dirname+'_DelphesForMa5tuneRun/Output/_'+item.name+'/TheMouth.root',\
                              dirname+'/Events/'+item.name+'_v1x1_'+card.replace('.tcl','')+'.root')
                        if not FolderWriter.RemoveDirectory(os.path.normpath(dirname+'_DelphesForMa5tuneRun')):
                            return False
                elif version in ['v1.2', 'v1.3']:
                    if not self.main.recasting.delphes:
                        self.logger.error('The Delphes library is not present... v1.2+ analyses cannot be used')
                        return False

                    if firstv13:
                        self.logger.info("   **********************************************************")
                        self.logger.info("   "+StringTools.Center('v1.2+ detector simulations',57))
                        self.logger.info("   **********************************************************")
                        firstv13=False

                    ## Deactivating delphesMA5tune
                    installer=InstallManager(self.main)
                    if not installer.Deactivate('delphesMA5tune'):
                        return False

                    ## Activating and compile Delphes
                    if installer.Activate('delphes')==-1:
                        return False

                    ## running delphesMA5tune
                    for item in self.main.datasets:
                        if not os.path.isfile(dirname+'/Events/' + item.name +\
                         '_v1x2_'+card.replace('.tcl','')+'.root'):
                            self.main.recasting.status="off"
                            self.main.fastsim.package="delphes"
                            self.main.fastsim.clustering=0
                            self.main.fastsim.delphesMA5tune=0
                            self.main.fastsim.delphes = DelphesConfiguration()
                            self.main.fastsim.delphes.card = os.path.normpath("../../../../PAD/Input/Cards/"+card)
                            self.submit(dirname+'_DelphesRun',[])
                            self.main.recasting.status="on"
                            self.main.fastsim.package="none"
                            ## saving the output
                            shutil.move(dirname+'_DelphesRun/Output/_'+item.name+'/TheMouth.root',\
                              dirname+'/Events/'+item.name+'_v1x2_'+card.replace('.tcl','')+'.root')
                        if not FolderWriter.RemoveDirectory(os.path.normpath(dirname+'_DelphesRun')):
                            return False
                else:
                    self.logger.error('An analysis can only be compatible with ma5 v1.1, v1.2 or v1.3...')
                    return False
            self.main.forced=forced_bkp

            ### Third, executing the analyses
            if not self.main.recasting.GetAnalysisRuns(dirname+"/Input/recasting_card.dat"):
                return False
            forced_bkp = self.main.forced
            self.main.forced=True
            for mydelphescard in list(set(sorted(self.main.recasting.delphesruns))):
                ## checking the PAD Version
                myversion  = mydelphescard[:4]
                if myversion=='v1.2':
                    PADdir = self.main.archi_info.ma5dir+'/PAD'
                elif myversion=='v1.1':
                    PADdir = self.main.archi_info.ma5dir+'/PADForMA5tune'
                else:
                    self.logger.error('Unknown PAD version')
                    self.main.forced=forced_bkp
                    return False
                ## current delphes card
                mycard     = mydelphescard[5:]
                ## print out
                self.logger.info("   **********************************************************")
                self.logger.info("   "+StringTools.Center(myversion+' running of the PAD'+\
                       ' on events generated with',57))
                self.logger.info("   "+StringTools.Center(mycard,57))
                self.logger.info("   **********************************************************")
                ## setting up Delphes
                installer=InstallManager(self.main)
                if myversion=='v1.1':
                    if not installer.Deactivate('delphes'):
                        self.main.forced=forced_bkp
                        return False
                    if installer.Activate('delphesMA5tune')==-1:
                        self.main.forced=forced_bkp
                        return False
                elif myversion=='v1.2':
                    if not installer.Deactivate('delphesMA5tune'):
                        self.main.forced=forced_bkp
                        return False
                    if installer.Activate('delphes')==-1:
                        self.main.forced=forced_bkp
                        return False
                ## Analyses
                myanalyses = self.main.recasting.analysisruns
                myanalyses = [ x for x in myanalyses if myversion in x ]
                myanalyses = [ x.replace(myversion+'_','') for x in myanalyses ]
                for card,analysislist in self.main.recasting.DelphesDic.items():
                      if card == mycard:
                          tmpanalyses=analysislist
                          break
                myanalyses = [ x for x in myanalyses if x in tmpanalyses]
                ## event file
                for myset in self.main.datasets:
                    myevents=os.path.normpath(dirname + '/Events/' + myset.name + '_' +\
                       myversion.replace('.','x')+'_' + mycard.replace('.tcl','')+'.root')
                    ## preparing and running the PAD
                    if not self.main.recasting.UpdatePADMain(myanalyses,PADdir):
                        self.main.forced=forced_bkp
                        return False
                    time.sleep(1.);
                    if not self.main.recasting.MakePAD(PADdir,dirname,self.main):
                        self.main.forced=forced_bkp
                        return False
                    time.sleep(1.);
                    if not self.main.recasting.RunPAD(PADdir,myevents):
                        self.main.forced=forced_bkp
                        return False
                    time.sleep(1.);
                    ## Restoring the PAD as it was before
                    if not self.main.recasting.RestorePADMain(PADdir,dirname,self.main):
                        self.main.forced=forced_bkp
                        return False
                    time.sleep(1.);
                    ## saving the output
                    if not self.main.recasting.SavePADOutput(PADdir,dirname,myanalyses,myset.name):
                        self.main.forced=forced_bkp
                        return False
                    if not self.main.recasting.store_root:
                        os.remove(os.path.normpath(dirname + '/Events/' + myset.name + '_' +\
                       myversion.replace('.','x')+'_' + mycard.replace('.tcl','')+'.root'))
                    time.sleep(1.);
                    ## Running the CLs exclusion script (if available)
                    if not self.main.recasting.GetCLs(PADdir,dirname,myanalyses,myset.name,myset.xsection,myset.name):
                        self.main.forced=forced_bkp
                        return False
                    ## Saving the results
            self.main.forced=forced_bkp
        else:
            self.logger.info("   Inserting your selection into 'SampleAnalyzer'...")
            if not jobber.WriteSelectionHeader(self.main):
                self.logger.error("job submission aborted.")
                return False
            if not jobber.WriteSelectionSource(self.main):
                self.logger.error("job submission aborted.")
                return False

        self.logger.info("   Writing the list of datasets...")
        for item in self.main.datasets:
            jobber.WriteDatasetList(item)

        self.logger.info("   Writing the command line history...")
        jobber.WriteHistory(history,self.main.firstdir)
        if self.main.recasting.status == "on":
            self.main.recasting.collect_outputs(dirname,self.main.datasets)
            self.logger.info('    -> the results can be found in:') 
            self.logger.info('       '+ dirname + '/Output/CLs_output_summary.dat')
            for item in self.main.datasets:
                self.logger.info('       '+ dirname + '/Output/'+ item.name + '/CLs_output.dat')
        else:
            layouter = LayoutWriter(self.main, dirname)
            layouter.WriteLayoutConfig()

        if not self.main.recasting.status=='on' and not self.resubmit:
            self.logger.info("   Creating Makefiles...")
            if not jobber.WriteMakefiles():
                self.logger.error("job submission aborted.")
                return False

        # Edit the delphes or recasting cards
        if self.main.fastsim.package in ["delphes","delphesMA5tune"]:
            self.editDelphesCard(dirname)

        if self.resubmit and not self.main.recasting.status=='on':
            self.logger.info("   Cleaning 'SampleAnalyzer'...")
            if not jobber.MrproperJob():
                self.logger.error("job submission aborted.")
                return False

        if not self.main.recasting.status=='on':
            self.logger.info("   Compiling 'SampleAnalyzer'...")
            if not jobber.CompileJob():
                self.logger.error("job submission aborted.")
                return False

            self.logger.info("   Linking 'SampleAnalyzer'...")
            if not jobber.LinkJob():
                self.logger.error("job submission aborted.")
                return False

            for item in self.main.datasets:
                self.logger.info("   Running 'SampleAnalyzer' over dataset '"
                             +item.name+"'...")
                self.logger.info("    *******************************************************")
                if not jobber.RunJob(item):
                    self.logger.error("run over '"+item.name+"' aborted.")
                self.logger.info("    *******************************************************")
        return True


    def extract(self,dirname,layout):
        self.logger.info("   Checking SampleAnalyzer output...")
        jobber = JobReader(dirname)
        if self.main.recasting.status=='on':
            if not self.main.recasting.CheckDir(dirname):
                return False
        elif not jobber.CheckDir():
            self.logger.error("errors have occured during the analysis.")
            return False

        for item in self.main.datasets:
            if self.main.recasting.status=='on':
                if not self.main.recasting.CheckFile(dirname,item):
                    return False
            elif not jobber.CheckFile(item):
                self.logger.error("errors have occured during the analysis.")
                return False

        if self.main.recasting.status!='on':
            self.logger.info("   Extracting data from the output files...")
            for i in range(0,len(self.main.datasets)):
                jobber.Extract(self.main.datasets[i],\
                               layout.cutflow.detail[i],\
                               0,\
                               layout.plotflow.detail[i],\
                               domerging=False)
                if self.main.merging.enable:
                    jobber.Extract(self.main.datasets[i],\
                                   0,\
                                   layout.merging.detail[i],\
                                   0,\
                                   domerging=True)
        return True


    def help(self):
        if not self.resubmit:
            self.logger.info("   Syntax: submit <dirname>")
            self.logger.info("   Performs an analysis over a list of datasets. Output is stored into the directory <dirname>.")
            self.logger.info("   If the optional argument is omitted, MadAnalysis creates a fresh directory automatically.")
            self.logger.info("   HTML and PDF reports are automatically created.")
        else:
            self.logger.info("   Syntax: resubmit")
            self.logger.info("   Update of an analysis already performed, if relevant.")
            self.logger.info("   In all cases, the HTML and PDF reports are regenerated.")


    def complete(self,text,line,begidx,endidx):

        #Resubmission case 
        if self.resubmit:
            return 

        #Getting back arguments
        args = line.split()
        nargs = len(args)
        if not text:
            nargs += 1
        
        #Checking number of arguments
        if nargs==2:
            output=[]
            for file in glob.glob(text+"*"):
                if os.path.isdir(file):
                    output.append(file)
            return self.finalize_complete(text,output)
        else:
            return []
