//
//  ViewController.swift
//  MXStackTrace
//
//  Created by 贺靖 on 2019/3/20.
//  Copyright © 2019 贺靖. All rights reserved.
//

import UIKit

class ViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        DispatchQueue.global().async {
            print(MXStatckTrace.getMainThreadStack())
        }
        // Do any additional setup after loading the view, typically from a nib.
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        
        DispatchQueue.global().async {
            print(MXStatckTrace.getMainThreadStack())
        }
        self.testA()
    }

    func testA() {
        testB()
    }
    
    func testB(){
        print("****")
    }

}

