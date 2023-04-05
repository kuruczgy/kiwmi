import React, { useState } from 'react'
import ReactDOM from 'react-dom/client'
import './index.css'
import App from './App'
import reportWebVitals from './reportWebVitals'
import NumberSelector from './components/NumberSelector'
import * as serviceWorkerRegistration from './serviceWorkerRegistration'

const root = ReactDOM.createRoot(document.getElementById('root') as HTMLElement)

const Frame = ({ children }: { children: any }) => {
  return <div className="main">{children}</div>
}

// const Main = () => {
//   const [value, setValue] = useState(3)
//   return <NumberSelector value={value} label="HDMI-2" onChange={setValue} />
// }

const Main = App

root.render(
  <Frame>
    <Main />
  </Frame>,
)

// If you want to start measuring performance in your app, pass a function
// to log results (for example: reportWebVitals(console.log))
// or send to an analytics endpoint. Learn more: https://bit.ly/CRA-vitals
// reportWebVitals()

serviceWorkerRegistration.register()
