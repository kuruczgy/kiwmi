import ReactDOM from 'react-dom/client'
import './index.scss'
import App from './App'
import * as serviceWorkerRegistration from './serviceWorkerRegistration'

const rootElement = document.getElementById('root') as HTMLElement
// rootElement.addEventListener(
//   'mousedown',
//   (e) => {
//     ;(e.metaKey as any) = false
//     e.
//   },
//   { capture: true },
// )

const root = ReactDOM.createRoot(rootElement)

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
