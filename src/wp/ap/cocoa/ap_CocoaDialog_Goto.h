/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2001, 2003 Hubert Figuiere
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  
 * 02111-1307, USA.
 */

#ifndef AP_COCOADIALOG_GOTO_H
#define AP_COCOADIALOG_GOTO_H

#import <Cocoa/Cocoa.h>

#include "ap_Dialog_Goto.h"
class XAP_CocoaFrame;
@class AP_CocoaDialog_GotoController;
@protocol XAP_CocoaDialogProtocol;

/*****************************************************************/

class AP_CocoaDialog_Goto: public AP_Dialog_Goto
{
public:
	AP_CocoaDialog_Goto(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id flgid);
	virtual ~AP_CocoaDialog_Goto(void);

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id dlgid);

	virtual void			runModeless(XAP_Frame * pFrame);
	virtual void			destroy(void);
	virtual void			activate(void);
	virtual void                    notifyActiveFrame(XAP_Frame *pFrame);
	void					setSelectedRow(int row);
	int						getSelectedRow(void);

	void					doGoto(const char *number);
	
	void 					event_goto(const char *number);
	void 					event_backward(void);
	void 					event_forward(void);
	void					event_valueChanged(void);
	void					event_targetChanged(int row);
	const char * getWindowName(void) { return m_WindowName; };
private:
	int m_iRow;
	AP_CocoaDialog_GotoController* m_dlg;
};


@interface AP_CocoaDialog_GotoController : NSWindowController <XAP_CocoaDialogProtocol>
{
    IBOutlet NSButton *backBtn;
    IBOutlet NSButton *forwardBtn;
    IBOutlet NSButton *jumpToBtn;
    IBOutlet NSComboBox *valueCombo;
    IBOutlet NSTextField *valueLabel;
    IBOutlet NSTextField *whatLabel;
    IBOutlet NSPopUpButton *whatPopup;
	AP_CocoaDialog_Goto* _xap;
}
- (NSString*)stringValue;
- (IBAction)backAction:(id)sender;
- (IBAction)closeAction:(id)sender;
- (IBAction)forwardAction:(id)sender;
- (IBAction)jumpToAction:(id)sender;
- (IBAction)valueComboAction:(id)sender;
- (IBAction)whatPopupAction:(id)sender;
@end

#endif /* AP_COCOADIALOG_GOTO_H */



